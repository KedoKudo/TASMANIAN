/*
 * Copyright (c) 2017, Miroslav Stoyanov
 *
 * This file is part of
 * Toolkit for Adaptive Stochastic Modeling And Non-Intrusive ApproximatioN: TASMANIAN
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * UT-BATTELLE, LLC AND THE UNITED STATES GOVERNMENT MAKE NO REPRESENTATIONS AND DISCLAIM ALL WARRANTIES, BOTH EXPRESSED AND IMPLIED.
 * THERE ARE NO EXPRESS OR IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, OR THAT THE USE OF THE SOFTWARE WILL NOT INFRINGE ANY PATENT,
 * COPYRIGHT, TRADEMARK, OR OTHER PROPRIETARY RIGHTS, OR THAT THE SOFTWARE WILL ACCOMPLISH THE INTENDED RESULTS OR THAT THE SOFTWARE OR ITS USE WILL NOT RESULT IN INJURY OR DAMAGE.
 * THE USER ASSUMES RESPONSIBILITY FOR ALL LIABILITIES, PENALTIES, FINES, CLAIMS, CAUSES OF ACTION, AND COSTS AND EXPENSES, CAUSED BY, RESULTING FROM OR ARISING OUT OF,
 * IN WHOLE OR IN PART THE USE, STORAGE OR DISPOSAL OF THE SOFTWARE.
 */

#ifndef __TASMANIAN_SPARSE_GRID_WAVELET_CPP
#define __TASMANIAN_SPARSE_GRID_WAVELET_CPP

#include "tsgGridWavelet.hpp"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace TasGrid{

GridWavelet::GridWavelet() : num_dimensions(0), num_outputs(0), order(1), points(0), needed(0), values(0), inter_matrix(0)
{}
GridWavelet::GridWavelet(const GridWavelet &wav) : num_dimensions(0), num_outputs(0), order(1), points(0), needed(0), values(0), inter_matrix(0)
{  copyGrid(&wav);  }
GridWavelet::~GridWavelet(){ reset(); }

void GridWavelet::reset(){
    if (points != 0){ delete points; points = 0; }
    if (needed != 0){ delete needed; needed = 0; }
    if (values != 0){ delete values; values = 0; }
    if (inter_matrix != 0){ delete inter_matrix; inter_matrix = 0; }
    coefficients.clear();
}

void GridWavelet::write(std::ofstream &ofs) const{
    using std::endl;

    ofs << std::scientific; ofs.precision(17);
    ofs << num_dimensions << " " << num_outputs << " " << order << endl;
    if (num_dimensions > 0){
        if (points == 0){
            ofs << "0" << endl;
        }else{
            ofs << "1 ";
            points->write(ofs);
        }
        if (coefficients.getTotalEntries() == 0){
            ofs << "0" << endl;
        }else{
            ofs << "1 ";
            for(auto c: *coefficients.getVector()) ofs << " " << c;
            ofs << endl;
        }
        if (needed == 0){
            ofs << "0" << endl;
        }else{
            ofs << "1 ";
            needed->write(ofs);
        }
        if (num_outputs > 0) values->write(ofs);
    }
}
void GridWavelet::writeBinary(std::ofstream &ofs) const{
    int dims[3];
    dims[0] = num_dimensions;
    dims[1] = num_outputs;
    dims[2] = order;
    ofs.write((char*) dims, 3 * sizeof(int));
    if (num_dimensions > 0){
        char flag;
        if (points == 0){
            flag = 'n'; ofs.write(&flag, sizeof(char));
        }else{
            flag = 'y'; ofs.write(&flag, sizeof(char));
            points->writeBinary(ofs);
        }
        if (needed == 0){
            flag = 'n'; ofs.write(&flag, sizeof(char));
        }else{
            flag = 'y'; ofs.write(&flag, sizeof(char));
            needed->writeBinary(ofs);
        }
        if (coefficients.getTotalEntries()  == 0){
            flag = 'n'; ofs.write(&flag, sizeof(char));
        }else{
            flag = 'y'; ofs.write(&flag, sizeof(char));
            ofs.write((char*) coefficients.getCStrip(0), coefficients.getTotalEntries() * sizeof(double));
        }
        if (num_outputs > 0) values->writeBinary(ofs);
    }
}
void GridWavelet::read(std::ifstream &ifs){
    reset();
    ifs >> num_dimensions >> num_outputs >> order;
    if (num_dimensions > 0){
        int flag;
        rule1D.updateOrder(order);

        ifs >> flag;
        if (flag == 1){
            points = new IndexSet(num_dimensions);
            points->read(ifs);
        }
        ifs >> flag;
        if (flag == 1){
            coefficients.resize(num_outputs, points->getNumIndexes());
            for(auto &c : *coefficients.getVector()) ifs >> c;
        }
        ifs >> flag;
        if (flag == 1){
            needed = new IndexSet(num_dimensions);
            needed->read(ifs);
        }

        if (num_outputs > 0){  values = new StorageSet(0, 0); values->read(ifs);  }
    }
    buildInterpolationMatrix();
}
void GridWavelet::readBinary(std::ifstream &ifs){
    int dims[3];
    ifs.read((char*) dims, 3 * sizeof(int));
    num_dimensions = dims[0];
    num_outputs = dims[1];
    order = dims[2];
    if (num_dimensions > 0){
        char flag;
        rule1D.updateOrder(order);

        ifs.read((char*) &flag, sizeof(char)); if (flag == 'y'){ points = new IndexSet(num_dimensions); points->readBinary(ifs); }
        ifs.read((char*) &flag, sizeof(char)); if (flag == 'y'){ needed = new IndexSet(num_dimensions); needed->readBinary(ifs); }

        ifs.read((char*) &flag, sizeof(char));
        if (flag == 'y'){
            coefficients.resize(num_outputs, points->getNumIndexes());
            ifs.read((char*) coefficients.getStrip(0), coefficients.getTotalEntries() * sizeof(double));
        }

        if (num_outputs > 0){ values = new StorageSet(0, 0); values->readBinary(ifs); }
    }
    buildInterpolationMatrix();
}

void GridWavelet::makeGrid(int cnum_dimensions, int cnum_outputs, int depth, int corder, const std::vector<int> &level_limits){
    reset();
    num_dimensions = cnum_dimensions;
    num_outputs = cnum_outputs;
    order = corder;

    rule1D.updateOrder(order);

    IndexManipulator IM(num_dimensions);
    IndexSet* deltas = IM.selectTensors(depth, type_level, std::vector<int>(), rule_leja);
    if (!level_limits.empty()){
        IndexSet *limited = IM.removeIndexesByLimit(deltas, level_limits);
        if (limited != 0){
            delete deltas;
            deltas = limited;
        }
    }

    if (order == 1){
        needed = IM.generatePointsFromDeltas(deltas, [](int level) -> int { return (1 << (level + 1)) + 1; });
    }else{
        needed = IM.generatePointsFromDeltas(deltas, [](int level) -> int { return (1 << (level + 2)) + 1; });
    }
    delete deltas;

    if (num_outputs == 0){
        points = needed;
        needed = 0;
    }else{
        values = new StorageSet(num_outputs, needed->getNumIndexes());
    }

    buildInterpolationMatrix();
}
void GridWavelet::copyGrid(const GridWavelet *wav){
    reset();
    num_dimensions = wav->num_dimensions;
    num_outputs = wav->num_outputs;
    order = wav->order;

    rule1D.updateOrder(order);

    if (wav->points != 0) points = new IndexSet(wav->points);
    if (wav->needed != 0) needed = new IndexSet(wav->needed);

    if (wav->values != 0) values = new StorageSet(wav->values);

    buildInterpolationMatrix();

    if ((points != 0) && (num_outputs > 0)){ // points are loaded
        recomputeCoefficients();
    }
}

void GridWavelet::setNodes(IndexSet* &nodes, int cnum_outputs, int corder){
    reset();
    num_dimensions = nodes->getNumDimensions();
    num_outputs = cnum_outputs;
    order = corder;

    rule1D.updateOrder(order);

    needed = nodes; nodes = 0;

    if (num_outputs == 0){
        points = needed;
        needed = 0;
    }else{
        values = new StorageSet(num_outputs, needed->getNumIndexes());
    }

    buildInterpolationMatrix();
}

int GridWavelet::getNumDimensions() const{ return num_dimensions;  }
int GridWavelet::getNumOutputs() const{ return num_outputs;  }
TypeOneDRule GridWavelet::getRule() const{ return rule_wavelet;  }
int GridWavelet::getOrder() const{ return order;  }

int GridWavelet::getNumLoaded() const{ return (((points == 0) || (num_outputs == 0)) ? 0 : points->getNumIndexes()); }
int GridWavelet::getNumNeeded() const{ return ((needed == 0) ? 0 : needed->getNumIndexes()); }
int GridWavelet::getNumPoints() const{ return ((points == 0) ? getNumNeeded() : points->getNumIndexes()); }

void GridWavelet::getLoadedPoints(double *x) const{
    int num_points = points->getNumIndexes();
    #pragma omp parallel for schedule(static)
    for(int i=0; i<num_points; i++){
        const int *p = points->getIndex(i);
        for(int j=0; j<num_dimensions; j++){
            x[i*num_dimensions + j] = rule1D.getNode(p[j]);
        }
    }
}
void GridWavelet::getNeededPoints(double *x) const{
    int num_points = needed->getNumIndexes();
    #pragma omp parallel for schedule(static)
    for(int i=0; i<num_points; i++){
        const int *p = needed->getIndex(i);
        for(int j=0; j<num_dimensions; j++){
            x[i*num_dimensions + j] = rule1D.getNode(p[j]);
        }
    }
}
void GridWavelet::getPoints(double *x) const{
    if (points == 0){ getNeededPoints(x); }else{ getLoadedPoints(x); }
}

void GridWavelet::getQuadratureWeights(double *weights) const{
    IndexSet *work = (points == 0) ? needed : points;
    int num_points = work->getNumIndexes();
	#pragma omp parallel for
	for(int i=0; i<num_points; i++){
		weights[i] = evalIntegral(work->getIndex(i));
	}
	solveTransposed(weights);
}
void GridWavelet::getInterpolationWeights(const double x[], double *weights) const{
    IndexSet *work = (points == 0) ? needed : points;
    int num_points = work->getNumIndexes();
	#pragma omp parallel for
	for(int i=0; i<num_points; i++){
        weights[i] = evalBasis(work->getIndex(i), x);
	}
	solveTransposed(weights);
}
void GridWavelet::loadNeededPoints(const double *vals, TypeAcceleration){
    if (points == 0){
        values->setValues(vals);
        points = needed;
        needed = 0;
    }else if (needed == 0){
        values->setValues(vals);
    }else{
        values->addValues(points, needed, vals);
        points->addIndexSet(needed);
        delete needed; needed = 0;
        buildInterpolationMatrix();
    }
    recomputeCoefficients();
}
void GridWavelet::mergeRefinement(){
    if (needed == 0) return; // nothing to do
    int num_all_points = getNumLoaded() + getNumNeeded();
    size_t size_vals = ((size_t) num_all_points) * ((size_t) num_outputs);
    std::vector<double> vals(size_vals, 0.0);
    values->setValues(vals);
    if (points == 0){
        points = needed;
        needed = 0;
    }else{
        points->addIndexSet(needed);
        delete needed; needed = 0;
        buildInterpolationMatrix();
    }
    coefficients.resize(num_outputs, num_all_points);
    std::fill(coefficients.getVector()->begin(), coefficients.getVector()->end(), 0.0);
}
void GridWavelet::evaluate(const double x[], double y[]) const{
    int num_points = points->getNumIndexes();
	double *basis_values = new double[num_points];
	#pragma omp parallel for
	for(int i=0; i<num_points; i++){
        basis_values[i] = evalBasis(points->getIndex(i), x);
	}
	for(int j=0; j<num_outputs; j++){
        double sum = 0.0;
        #pragma omp parallel for reduction(+ : sum)
        for(int i=0; i<num_points; i++){
            sum += basis_values[i] * coefficients.getCStrip(i)[j];
        }
        y[j] = sum;
	}
	delete[] basis_values;
}

void GridWavelet::evaluateFastCPUblas(const double x[], double y[]) const{ evaluate(x, y); }
void GridWavelet::evaluateFastGPUcublas(const double x[], double y[]) const{ evaluate(x, y); }
void GridWavelet::evaluateFastGPUcuda(const double x[], double y[]) const{ evaluate(x, y); }
void GridWavelet::evaluateFastGPUmagma(int, const double x[], double y[]) const{ evaluate(x, y); }

void GridWavelet::evaluateBatch(const double x[], int num_x, double y[]) const{
    #pragma omp parallel for
    for(int i=0; i<num_x; i++){
        evaluate(&(x[i*num_dimensions]), &(y[i*num_outputs]));
    }
}
void GridWavelet::evaluateBatchCPUblas(const double x[], int num_x, double y[]) const{
    evaluateBatch(x, num_x, y);
}
void GridWavelet::evaluateBatchGPUcublas(const double x[], int num_x, double y[]) const{
    evaluateBatch(x, num_x, y);
}
void GridWavelet::evaluateBatchGPUcuda(const double x[], int num_x, double y[]) const{
    evaluateBatch(x, num_x, y);
}
void GridWavelet::evaluateBatchGPUmagma(int, const double x[], int num_x, double y[]) const{
    evaluateBatch(x, num_x, y);
}

void GridWavelet::integrate(double q[], double *conformal_correction) const{
    int num_points = points->getNumIndexes();

    if (conformal_correction == 0){
        double *basis_integrals = new double[num_points];
        #pragma omp parallel for
        for(int i=0; i<num_points; i++){
            basis_integrals[i] = evalIntegral(points->getIndex(i));
        }
        for(int j=0; j<num_outputs; j++){
            double sum = 0.0;
            #pragma omp parallel for reduction(+ : sum)
            for(int i=0; i<num_points; i++){
                sum += basis_integrals[i] * coefficients.getCStrip(i)[j];
            }
            q[j] = sum;
        }
        delete[] basis_integrals;
    }else{
        std::fill(q, q + num_outputs, 0.0);
        double *w = new double[num_points];
        getQuadratureWeights(w);
        for(int i=0; i<num_points; i++){
            w[i] *= conformal_correction[i];
            const double *vals = values->getValues(i);
            for(int k=0; k<num_outputs; k++){
                q[k] += w[i] * vals[k];
            }
        }
        delete[] w;
    }
}

double GridWavelet::evalBasis(const int p[], const double x[]) const{
	/*
	 * Evaluates the wavelet basis given at point p at the coordinates given by x.
	 */
	double v = 1.0;
	for(int i = 0; i < num_dimensions; i++){
		v *= rule1D.eval(p[i], x[i]);
		if (v == 0.0){ break; }; // MIRO: reduce the expensive wavelet evaluations
	}
	return v;
}
double GridWavelet::evalIntegral(const int p[]) const{
	/*
	 * For a given node p, evaluate the integral of the associated wavelet.
	 */
	double v = 1.0;
	for(int i = 0; i < num_dimensions; i++){
		v *= rule1D.getWeight(p[i]);
		if (v == 0.0){ break; }; // MIRO: reduce the expensive wavelet evaluations
	}
	return v;
}

void GridWavelet::buildInterpolationMatrix(){
    // updated code, using better parallelism
    // Wavelets don't have a nice rule of support to use monkeys and graphs (or I cannot find the support rule)
    IndexSet *work = (points == 0) ? needed : points;
    if(inter_matrix != 0) { delete inter_matrix; }

    int num_points = work->getNumIndexes();

    int num_chunk = 32;
    int num_blocks = num_points / num_chunk + ((num_points % num_chunk == 0) ? 0 : 1);

    std::vector<std::vector<int>> indx(num_blocks);
    std::vector<std::vector<double>> vals(num_blocks);
    std::vector<int> pntr(num_points);

    #pragma omp parallel for
    for(int b=0; b<num_blocks; b++){
        int block_end = (b < num_blocks - 1) ? (b+1) * num_chunk : num_points;
        for(int i=b * num_chunk; i < block_end; i++){
            const int *p = work->getIndex(i);
            std::vector<double> xi(num_dimensions);
            for(int j = 0; j<num_dimensions; j++) // get the node
                xi[j] = rule1D.getNode(p[j]);

            // loop over the basis functions to see if supported
            int numpntr = 0;
            for(int wi=0; wi<num_points; wi++){
                const int *w = work->getIndex(wi);

                double v = 1.0;
                for(int j=0; j<num_dimensions; j++){
                    v *= rule1D.eval(w[j], xi[j]);
                    if (v == 0.0) break; // evaluating the wavelets is expensive, stop if any one of them is zero
                }

                if(v != 0.0){ // testing != is safe since it can only happen in multiplication by 0.0
                    indx[b].push_back(wi);
                    vals[b].push_back(v);
                    numpntr++;
                }
            }
            pntr[i] = numpntr;
        }
    }

    inter_matrix = new TasSparse::SparseMatrix(pntr, indx, vals);
}

void GridWavelet::recomputeCoefficients(){
	/*
	 * Recalculates the coefficients to interpolate the values in points.
	 * Make sure buildInterpolationMatrix has been called since the list was updated.
	 */

	int num_points = points->getNumIndexes();
	coefficients.resize(num_outputs, num_points);

	if ((inter_matrix == 0) || (inter_matrix->getNumRows() != num_points)){
        buildInterpolationMatrix();
	}

	double *workspace = new double[2*num_points];
	double *b = workspace;
	double *x = workspace + num_points;

	for(int output = 0; output < num_outputs; output++){
		// Copy relevant portion
		std::fill(workspace, workspace + 2*num_points, 0.0);

		// Populate RHS
		for(int i = 0; i < num_points; i++){
			b[i] = values->getValues(i)[output];
		}

		// Solve system
		inter_matrix->solve(b, x);

		// Populate surplus
		for(int i = 0; i < num_points; i++){
            coefficients.getStrip(i)[output] = x[i];
		}
	}

	delete[] workspace;
}

void GridWavelet::solveTransposed(double w[]) const{
	/*
	 * Solves the system A^T * w = y. Used to calculate interpolation and integration
	 * weights. RHS values should be passed in through w. At exit, w will contain the
	 * required weights.
	 */
	int num_points = inter_matrix->getNumRows();

	double *y = new double[num_points];

	std::copy(w, w + num_points, y);

	inter_matrix->solve(y, w, true);

	delete[] y;
}

void GridWavelet::getNormalization(std::vector<double> &norm) const{
    norm.resize(num_outputs);
    std::fill(norm.begin(), norm.end(), 0.0);
    for(int i=0; i<points->getNumIndexes(); i++){
        const double *v = values->getValues(i);
        for(int j=0; j<num_outputs; j++){
            if (norm[j] < fabs(v[j])) norm[j] = fabs(v[j]);
        }
    }
}
int* GridWavelet::buildUpdateMap(double tolerance, TypeRefinement criteria, int output) const{
    int num_points = points->getNumIndexes();
    int *pmap = new int[num_points * num_dimensions];  std::fill(pmap, pmap + num_points * num_dimensions, 0);

    std::vector<double> norm;
    getNormalization(norm);

    if ((criteria == refine_classic) || (criteria == refine_parents_first)){
        // classic refinement
        #pragma omp parallel for
        for(int i=0; i<num_points; i++){
            bool small = true;
            const double *s = coefficients.getCStrip(i);
            if (output == -1){
                for(size_t k=0; k<((size_t) num_outputs); k++){
                    if (small && ((fabs(s[k]) / norm[k]) > tolerance)) small = false;
                }
            }else{
                small = !((fabs(s[output]) / norm[output]) > tolerance);
            }
            if (!small){
                std::fill(&(pmap[i*num_dimensions]), &(pmap[i*num_dimensions]) + num_dimensions, 1);
            }
        }
    }else{
        SplitDirections split(points);

        for(int s=0; s<split.getNumJobs(); s++){
            int d = split.getJobDirection(s);
            int nump = split.getJobNumPoints(s);
            const int *pnts = split.getJobPoints(s);

            int *global_to_pnts = new int[num_points];

            int active_outputs = (output == -1) ? num_outputs : 1;

            double *vals = new double[nump * active_outputs];
            std::vector<int> indexes(nump * num_dimensions);

            for(int i=0; i<nump; i++){
                const double* v = values->getValues(pnts[i]);
                if (output == -1){
                    std::copy(v, v + num_outputs, &(vals[((size_t) i) * ((size_t) num_outputs)]));
                }else{
                    vals[i] = v[output];
                }
                const int *p = points->getIndex(pnts[i]);
                std::copy(p, p + num_dimensions, &(indexes[((size_t) i) * ((size_t) num_dimensions)]));
                global_to_pnts[pnts[i]] = i;
            }
            IndexSet *pointset = new IndexSet(num_dimensions, indexes);

            GridWavelet direction_grid;
            direction_grid.setNodes(pointset, active_outputs, order);
            direction_grid.loadNeededPoints(vals, accel_none);

            for(int i=0; i<nump; i++){
                bool small = true;
                const double *coeff = direction_grid.coefficients.getCStrip(i);
                const double *soeff = coefficients.getCStrip(pnts[i]);
                if (output == -1){
                    for(int k=0; k<num_outputs; k++){
                        if (small && ((fabs(soeff[k]) / norm[k]) > tolerance) && ((fabs(coeff[k]) / norm[k]) > tolerance)) small = false;
                    }
                }else{
                    if (((fabs(soeff[output]) / norm[output]) > tolerance) && ((fabs(coeff[0]) / norm[output]) > tolerance)) small = false;
                }
                pmap[pnts[i]*num_dimensions + d] = (small) ? 0 : 1;;
            }

            delete[] vals;
            delete[] global_to_pnts;
        }
    }

    return pmap;
}

bool GridWavelet::addParent(const int point[], int direction, GranulatedIndexSet *destination, IndexSet *exclude) const{
    std::vector<int> dad(num_dimensions);  std::copy(point, point + num_dimensions, dad.data());
    bool added = false;
    dad[direction] = rule1D.getParent(point[direction]);
    if (dad[direction] == -2){
        for(int c=0; c<rule1D.getNumPoints(0); c++){
            dad[direction] = c;
            if (exclude->getSlot(dad) == -1){
                destination->addIndex(dad);
                added = true;
            }
        }
    }else if (dad[direction] >= 0){
        if (exclude->getSlot(dad) == -1){
            destination->addIndex(dad);
            added = true;
        }
    }
    return added;
}
void GridWavelet::addChild(const int point[], int direction, GranulatedIndexSet *destination, IndexSet *exclude) const{
    std::vector<int> kid(num_dimensions);  std::copy(point, point + num_dimensions, kid.data());
    int L, R; rule1D.getChildren(point[direction], L, R);
    kid[direction] = L;
    if ((kid[direction] != -1) && (exclude->getSlot(kid) == -1)){
        destination->addIndex(kid);
    }
    kid[direction] = R;
    if ((kid[direction] != -1) && (exclude->getSlot(kid) == -1)){
        destination->addIndex(kid);
    }
}
void GridWavelet::addChildLimited(const int point[], int direction, GranulatedIndexSet *destination, IndexSet *exclude, const std::vector<int> &level_limits) const{
    std::vector<int> kid(num_dimensions);  std::copy(point, point + num_dimensions, kid.data());
    int L, R; rule1D.getChildren(point[direction], L, R);
    kid[direction] = L;
    if ((kid[direction] != -1) && (rule1D.getLevel(L) <= level_limits[direction]) && (exclude->getSlot(kid) == -1)){
        destination->addIndex(kid);
    }
    kid[direction] = R;
    if ( (kid[direction] != -1) && (rule1D.getLevel(R) <= level_limits[direction]) && (exclude->getSlot(kid) == -1)){
        destination->addIndex(kid);
    }
}

void GridWavelet::clearRefinement(){
    if (needed != 0){ delete needed;  needed = 0; }
}
const double* GridWavelet::getSurpluses() const{
    return coefficients.getCStrip(0);
}

void GridWavelet::evaluateHierarchicalFunctions(const double x[], int num_x, double y[]) const{
    IndexSet *work = (points == 0) ? needed : points;
    int num_points = work->getNumIndexes();
    for(int i=0; i<num_x; i++){
        double *this_y = &(y[i*num_points]);
        const double *this_x = &(x[i*num_dimensions]);
        for(int j=0; j<num_points; j++){
            const int* p = work->getIndex(j);
            double v = 1.0;
            for(int k=0; k<num_dimensions; k++){
                v *= rule1D.eval(p[k], this_x[k]);
                if (v == 0.0){ break; }; // MIRO: evaluating the wavelets is expensive, stop if any one of them is zero
            }
            this_y[j] = v;
        }
    }
}

void GridWavelet::setHierarchicalCoefficients(const double c[], TypeAcceleration acc){
    std::vector<double> *vals = 0;
    int num_ponits = getNumPoints();
    size_t size_coeff = ((size_t) num_ponits) * ((size_t) num_outputs);
    if (points != 0){
        clearRefinement();
    }else{
        points = needed;
        needed = 0;
    }
    vals = values->aliasValues();
    vals->resize(size_coeff);
    coefficients.resize(num_outputs, num_ponits);
    std::copy(c, c + size_coeff, coefficients.getStrip(0));
    std::vector<double> x(((size_t) num_ponits) * ((size_t) num_dimensions));
    getPoints(x.data());
    if (acc == accel_cpu_blas){
        evaluateBatchCPUblas(x.data(), points->getNumIndexes(), vals->data());
    }else if (acc == accel_gpu_cublas){
        evaluateBatchGPUcublas(x.data(), points->getNumIndexes(), vals->data());
    }else if (acc == accel_gpu_cuda){
        evaluateBatchGPUcuda(x.data(), points->getNumIndexes(), vals->data());
    }else{
        evaluateBatch(x.data(), points->getNumIndexes(), vals->data());
    }
}

const int* GridWavelet::getPointIndexes() const{
    return ((points == 0) ? needed->getIndex(0) : points->getIndex(0));
}
void GridWavelet::setSurplusRefinement(double tolerance, TypeRefinement criteria, int output, const std::vector<int> &level_limits){
    clearRefinement();

    int *pmap = buildUpdateMap(tolerance, criteria, output);

    bool useParents = (criteria == refine_fds) || (criteria == refine_parents_first);

    GranulatedIndexSet *refined = new GranulatedIndexSet(num_dimensions);

    int num_points = points->getNumIndexes();

    if (level_limits.empty()){
        for(int i=0; i<num_points; i++){
            for(int j=0; j<num_dimensions; j++){
                if (pmap[i*num_dimensions+j] == 1){ // if this dimension needs to be refined
                    if (!(useParents && addParent(points->getIndex(i), j, refined, points))){
                        addChild(points->getIndex(i), j, refined, points);
                    }
                }
            }
        }
    }else{
        for(int i=0; i<num_points; i++){
            for(int j=0; j<num_dimensions; j++){
                if (pmap[i*num_dimensions+j] == 1){ // if this dimension needs to be refined
                    if (!(useParents && addParent(points->getIndex(i), j, refined, points))){
                        addChildLimited(points->getIndex(i), j, refined, points, level_limits);
                    }
                }
            }
        }
    }

    if (refined->getNumIndexes() > 0){
        needed = new IndexSet(refined);
    }
    delete refined;

    delete[] pmap;
}

void GridWavelet::clearAccelerationData(){}

}

#endif
