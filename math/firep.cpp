/**********************************************************************
Copyright 2014-2016 The RIVET Developers. See the COPYRIGHT file at
the top-level directory of this distribution.

This file is part of RIVET.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
**********************************************************************/

#include "firep.h"

#include "index_matrix.h"
#include "map_matrix.h"

#include "debug.h"

#include <algorithm>
#include <limits> //std::numeric_limits
#include <sstream>
#include <stdexcept>

//FIRep constructor; requires dimension of homology to be computed and verbosity parameter
FIRep::FIRep(BifiltrationData& bd, int v)
    : hom_dim(bd.hom_dim), verbosity(v), x_grades(bd.num_x_grades()), y_grades(bd.num_y_grades()), bifiltration_data(bd)
{
    if (verbosity >= 10) {
        bd.print_bifiltration();
    }

    //Create the boundary matrices and index lists
    //First start in dim-1
    std::vector<AppearanceGrades*> low_simplices;
    SimplexInfo* simplices = bd.getSimplices(hom_dim - 1);
    SimplexInfo::iterator it;
    for(it = simplices->begin(); it != simplices->end(); it++)
    {
        low_simplices.push_back(&(it->second));
    }

    //Lambda function to sort appearance grades based on gca
    auto compareLowAppearanceGrades = [](const AppearanceGrades* left, const AppearanceGrades* right)
    {
        //Each simplex is replaced with the lowest common ancestor, and we sort by that
        //y index of lowest is y index of first grade of appearance, x index is the x index of last grade of appearanace
        if ((left->begin())->y != (right->begin())->y)
            return (left->begin())->y < (right->begin())->y;
        else
            return (left->back()).x < (right->back()).x;
    };
    std::sort(low_simplices.begin(), low_simplices.end(), compareLowAppearanceGrades);
    for (unsigned i = 0; i < low_simplices.size(); i++)
    {
        (*low_simplices[i])[0].dim_index = i; //Store order in the first simplex
    }

    //Now deal with dimension dim
    std::vector<Grade*> mid_simplex_grades;
    simplices = bd.getSimplices(hom_dim);
    for (it = simplices->begin(); it != simplices->end(); it++)
    {
        for (AppearanceGrades::iterator it2 = (it->second).begin(); it2 != (it->second).end(); it2++)
        {
            mid_simplex_grades.push_back(&(*it2));
        }
    }

    auto compareGradePointers = [](const Grade* left, const Grade* right) { return (*left) < (*right); };
    std::sort(mid_simplex_grades.begin(), mid_simplex_grades.end(), compareGradePointers);
    indexes_0.clear();
    if (verbosity >= 10)
    {
        debug() << "Printing generators of dimension " << hom_dim;
    }
    for (unsigned i = 0; i < mid_simplex_grades.size(); i++)
    {
        mid_simplex_grades[i]->dim_index = i;
        indexes_0.push_back(*(mid_simplex_grades[i]));
        if (verbosity >= 10)
        {
            debug() << mid_simplex_grades[i]->x << mid_simplex_grades[i]->y;
        }
    }

    //Now to make the boundary matrix
    //create the MapMatrix
    boundary_mx_0 = new MapMatrix(low_simplices.size(), mid_simplex_grades.size());

    //loop through simplices, writing columns to the matrix
    for (it = simplices->begin(); it != simplices->end(); it++)
    {
        for (AppearanceGrades::iterator it2 = (it->second).begin(); it2 != (it->second).end(); it2++)
        {
            write_boundary_column(boundary_mx_0, it->first, bd.getSimplices(hom_dim - 1), (*it2).dim_index);
        }
    }

    //Finally deal with dimension dim+1
    //structure which maintains what the boundary of a grade is (in the case of the relations)
    struct generator
    {
        int x;
        int y;
        std::vector<int> boundary;

        bool operator<(generator other) const
        {
            if (y != other.y)
                return y < other.y;
            else
                return x < other.x;
        }
    };

    std::vector<generator> high_generators;
    //Add the relations
    for (it = simplices->begin(); it != simplices->end(); it++)
    {
        for (AppearanceGrades::iterator it2 = (it->second).begin(); (it2 + 1) != (it->second).end(); it2++)
        {
            generator g;
            g.x = it2->x;
            g.y = (it2 + 1)->y;
            g.boundary.push_back(it2->dim_index);
            g.boundary.push_back((it2 + 1)->dim_index);
            high_generators.push_back(g);
        }
    }
    //Add generators of dim+1
    SimplexInfo* high_simplices = bd.getSimplices(hom_dim + 1);
    for (it = high_simplices->begin(); it != high_simplices->end(); it++)
    {
        std::vector<int> vertices = it->first;
        std::vector<AppearanceGrades*> facets;

        //find all facets of this simplex
        for (unsigned k = 0; k < vertices.size(); k++)
        {
            //facet vertices are all vertices in verts[] except verts[k]
            std::vector<int> facet;
            for (unsigned l = 0; l < vertices.size(); l++)
                if (l != k)
                    facet.push_back(vertices[l]);

            //look up dimension index of the facet
            SimplexInfo::iterator facet_node = simplices->find(facet);
            if (facet_node == simplices->end())
                throw std::runtime_error("FIRep::FIRep: Facet simplex not found.");
            facets.push_back(&facet_node->second);
        }

        for (AppearanceGrades::iterator it2 = it->second.begin(); it2 != it->second.end(); it2++)
        {
            generator g;
            g.x = it2->x;
            g.y = it2->y;
            for (unsigned k = 0; k < facets.size(); k++)
            {
                //Find facet that was generated before this grade
                unsigned l;
                for (l = 0; l < facets[k]->size(); l++)
                {
                    Grade * grade = &facets[k]->at(l);
                    if (grade->x <= g.x && grade->y <= g.y)
                    {
                        g.boundary.push_back(grade->dim_index);
                        break;
                    }
                }
                if (l == facets[k]->size())
                    throw std::runtime_error("FIRep::FIRep: Facet simplices not born before simplex.");
            }
            high_generators.push_back(g);
        }
    }

    //Sort and create matrices
    std::sort(high_generators.begin(), high_generators.end());
    indexes_1.clear();
    if (verbosity >= 10)
    {
        debug() << "Printing generators of dimension " << hom_dim + 1;
    }
    for (unsigned i = 0; i < high_generators.size(); i++)
    {
        Grade grade(high_generators[i].x, high_generators[i].y);
        grade.dim_index = i;
        indexes_1.push_back(grade);
        if (verbosity >= 10)
        {
            Debug qd = debug(true);
            qd << high_generators[i].x << " " << high_generators[i].y << " ; ";
            for (unsigned j = 0; j < high_generators[i].boundary.size(); j++)
                qd << high_generators[i].boundary[j] << " ";
            qd << "\n";
        }
    }

    //Now to make the boundary matrix
    //create the MapMatrix
    boundary_mx_1 = new MapMatrix(indexes_0.size(), indexes_1.size());

    //loop through generators, writing columns to the matrix
    for (unsigned i = 0; i < high_generators.size(); i++)
    {
        write_boundary_column(boundary_mx_1, high_generators[i].boundary, i);
    }

    if (verbosity >= 8) {
        debug() << "Created FIRep";
    }
    if (verbosity >= 10) {
        boundary_mx_0->print();
        boundary_mx_1->print();
    }
}

//destructor
FIRep::~FIRep()
{
    free(boundary_mx_0);
    free(boundary_mx_1);
}

//returns a matrix of boundary information for simplices of the given dimension (with multi-grade info)
//columns ordered according to dimension index (reverse-lexicographic order with respect to multi-grades)
MapMatrix* FIRep::get_boundary_mx(int dim)
{
    if (dim == hom_dim) {
        return boundary_mx_0;
    } else if (dim == hom_dim + 1) {
        return boundary_mx_1;
    } else {
        std::stringstream ss;
        ss << "FIRep::get_boundary_mx(): Attempting to compute boundary matrix for improper dimension (" << dim << "), expected either "
           << hom_dim << " or " << hom_dim + 1;
        throw std::runtime_error(ss.str());
    }
} //end get_boundary_mx(int)

//returns a boundary matrix for hom_dim-simplices with columns in a specified order -- for vineyard-update algorithm
//    simplex_order is a map : dim_index --> order_index for simplices of the given dimension
//        if simplex_order[i] == -1, then simplex with dim_index i is NOT represented in the boundary matrix
//    num_simplices is the number of simplices in the order (i.e., the number of entries in the vector that are NOT -1)
MapMatrix_Perm* FIRep::get_boundary_mx(std::vector<int>& coface_order, unsigned num_simplices)
{
    //create the matrix
    MapMatrix_Perm* mat = new MapMatrix_Perm(boundary_mx_0->height(), num_simplices);

    //loop through all simplices, writing columns to the matrix
    for (unsigned i = 0; i < boundary_mx_0->width(); i++)
    {
        int order_index = coface_order[i]; //index of the matrix column which will store the boundary of this simplex
        if (order_index != -1) {
            mat->add_column(boundary_mx_0, i, order_index);
        }
    }

    //return the matrix
    return mat;
} //end get_boundary_mx(int, vector<int>)

//returns a boundary matrix for (hom_dim+1)-simplices with columns and rows a specified orders -- for vineyard-update algorithm
//  PARAMETERS:
//    each vector is a map : dim_index --> order_index for simplices of the given dimension
//        if order[i] == -1, then simplex with dim_index i is NOT represented in the boundary matrix
//    each unsigned is the number of simplices in the corresponding order (i.e., the number of entries in the vector that are NOT -1)
MapMatrix_Perm* FIRep::get_boundary_mx(std::vector<int>& face_order, unsigned num_faces, std::vector<int>& coface_order, unsigned num_cofaces)
{
    //create the matrix
    MapMatrix_Perm* mat = new MapMatrix_Perm(num_faces, num_cofaces);

    for (unsigned i = 0; i < boundary_mx_1->width(); i++)
    {
        int order_index = coface_order[i]; //index of the matrix column which will store the boundary of this simplex
        if (order_index != -1) {
            MapMatrix::MapMatrixNode* node = boundary_mx_1->columns[i];
            while (node != NULL)
            {
                mat->set(face_order[node->get_row()], order_index);
                node = node->get_next();
            }
        }
    }

    //return the matrix
    return mat;
} //end get_boundary_mx(int, vector<int>, vector<int>)

//writes boundary information for simplex represented by sim in column col of matrix mat
void FIRep::write_boundary_column(MapMatrix* mat, const std::vector<int>& vertices, SimplexInfo* low_simplices, int col)
{
    //for a 0-simplex, there is nothing to do
    if (vertices.size() == 1)
        return;

    //find all facets of this simplex
    for (unsigned k = 0; k < vertices.size(); k++) {
        //facet vertices are all vertices in verts[] except verts[k]
        std::vector<int> facet;
        for (unsigned l = 0; l < vertices.size(); l++)
            if (l != k)
                facet.push_back(vertices[l]);

        //look up dimension index of the facet
        SimplexInfo::iterator facet_node = low_simplices->find(facet);
        if (facet_node == low_simplices->end())
            throw std::runtime_error("FIRep::write_boundary_column(): Facet simplex not found.");

        //for this boundary simplex, enter "1" in the appropriate cell in the matrix
        mat->set(facet_node->second.at(0).dim_index, col);
    }
} //end write_col();

//writes boundary information given boundary entries in column col of matrix mat
void FIRep::write_boundary_column(MapMatrix* mat, const std::vector<int>& entries, int col)
{
    //find all facets of this simplex
    for (unsigned k = 0; k < entries.size(); k++) {
        //for this boundary simplex, enter "1" in the appropriate cell in the matrix
        mat->set(entries[k], col);
    }
} //end write_col();

//returns a matrix of column indexes to accompany MapMatrices
//  entry (i,j) gives the last column of the MapMatrix that corresponds to multigrade (i,j)
IndexMatrix* FIRep::get_index_mx(int dim)
{
    //select set of simplices of dimension dim
    AppearanceGrades* grades;

    if (dim == hom_dim)
        grades = &indexes_0;
    else if (dim == hom_dim + 1)
        grades = &indexes_1;
    else
        throw std::runtime_error("FIRep::get_index_mx(): Attempting to compute index matrix for improper dimension.");

    //create the IndexMatrix
    unsigned x_size = x_grades;
    unsigned y_size = y_grades;
    IndexMatrix* mat = new IndexMatrix(y_size, x_size); //DELETE this object later!

    if (!grades->empty()) //then there is at least one simplex
    {
        //initialize to -1 the entries of end_col matrix before multigrade of the first simplex
        unsigned cur_entry = 0; //tracks previously updated multigrade in end-cols

        unsigned cur_grade = 0;
        for (; cur_entry < grades->at(0).x + grades->at(0).y * x_size; cur_entry++)
            mat->set(cur_entry / x_size, cur_entry % x_size, -1);

        //loop through simplices
        for (; cur_grade < grades->size(); cur_grade++) {
            int cur_x = grades->at(cur_grade).x;
            int cur_y = grades->at(cur_grade).y;

            //if some multigrades were skipped, store previous column number in skipped cells of end_col matrix
            for (; cur_entry < cur_x + cur_y * x_size; cur_entry++)
                mat->set(cur_entry / x_size, cur_entry % x_size, cur_grade - 1);

            //store current column number in cell of end_col matrix for this multigrade
            mat->set(cur_y, cur_x, cur_grade);
        }

        //store final column number for any cells in end_cols not yet updated
        for (; cur_entry < x_size * y_size; cur_entry++)
            mat->set(cur_entry / x_size, cur_entry % x_size, cur_grade - 1);
    } else //then there are no simplices, so fill the IndexMatrix with -1 values
    {
        for (unsigned i = 0; i < x_size * y_size; i++)
            mat->set(i / x_size, i % x_size, -1);
    }

    //return the matrix
    return mat;
} //end get_index_mx()

//returns the number of simplices of dimension (hom_dim-1), hom_dim, or (hom_dim+1)
unsigned FIRep::get_size(int dim)
{
    if (dim == hom_dim - 1)
        return boundary_mx_0->height();
    else if (dim == hom_dim)
        return indexes_0.size();
    else if (dim == hom_dim + 1)
        return indexes_1.size();
    else
        throw std::runtime_error("FIRep::get_size(): invalid dimension");
}

//returns the number of unique x-coordinates of the multi-grades
unsigned FIRep::num_x_grades()
{
    return x_grades;
}

//returns the number of unique y-coordinates of the multi-grades
unsigned FIRep::num_y_grades()
{
    return y_grades;
}