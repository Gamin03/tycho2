//----------------------------------*-C++-*----------------------------------//
/*!
 *  SweepData2.hh
 *
 *  Class for data for sweeps for use by PBJ and Schur algorithms.  Want the ability to change order of data
 *         data structures. The 2 distinguishes it from the SweepData for sweeps.
 */
//---------------------------------------------------------------------------//
// $Id: Mat.hh,v 1.17 2003/03/14 19:55:03 tme Exp $
//---------------------------------------------------------------------------//

/*
Copyright (c) 2016, Los Alamos National Security, LLC
All rights reserved.

Copyright 2016. Los Alamos National Security, LLC. This software was produced 
under U.S. Government contract DE-AC52-06NA25396 for Los Alamos National 
Laboratory (LANL), which is operated by Los Alamos National Security, LLC for 
the U.S. Department of Energy. The U.S. Government has rights to use, 
reproduce, and distribute this software.  NEITHER THE GOVERNMENT NOR LOS 
ALAMOS NATIONAL SECURITY, LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR 
ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE.  If software is modified 
to produce derivative works, such modified software should be clearly marked, 
so as not to confuse it with the version available from LANL.

Additionally, redistribution and use in source and binary forms, with or 
without modification, are permitted provided that the following conditions 
are met:
1.      Redistributions of source code must retain the above copyright notice, 
        this list of conditions and the following disclaimer.
2.      Redistributions in binary form must reproduce the above copyright 
        notice, this list of conditions and the following disclaimer in the 
        documentation and/or other materials provided with the distribution.
3.      Neither the name of Los Alamos National Security, LLC, Los Alamos 
        National Laboratory, LANL, the U.S. Government, nor the names of its 
        contributors may be used to endorse or promote products derived from 
        this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY LOS ALAMOS NATIONAL SECURITY, LLC AND 
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT 
NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LOS ALAMOS NATIONAL 
SECURITY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __SWEEP_DATA_2_HH__
#define __SWEEP_DATA_2_HH__


#include "Assert.hh"
#include "Typedef.hh"
#include <stddef.h>
#include <omp.h>

/*
    SweepData
    
    Holds psi and other data for the sweep.
*/
class SweepData2 : public TraverseData
{
public:
    
    /*
        Constructor
    */
    SweepData2(PsiData &psi, const PsiData &source, PsiBoundData &psiBound, 
              const double sigmaTotal)
    : c_psi(psi), c_psiBound(psiBound), 
      c_source(source), c_sigmaTotal(sigmaTotal),
      c_localFaceData(g_nThreads)
    {
        for (UINT angleGroup = 0; angleGroup < g_nThreads; angleGroup++) {
            c_localFaceData[angleGroup] = Mat2<double>(g_nVrtxPerFace, g_nGroups);
        }
    }
    
    /*
        data
        
        Return face data for (cell, angle) pair.
    */
    virtual const char* getData(UINT cell, UINT face, UINT angle)
    {
        Mat2<double> &localFaceData = c_localFaceData[omp_get_thread_num()];
        
        for (UINT group = 0; group < g_nGroups; group++) {
        for (UINT fvrtx = 0; fvrtx < g_nVrtxPerFace; fvrtx++) {
            UINT vrtx = g_tychoMesh->getFaceToCellVrtx(cell, face, fvrtx);
            localFaceData(fvrtx, group) = c_psi(group, vrtx, angle, cell);
        }}
        
        return (char*) (&localFaceData[0]);
    }
       
        
    /*
        getDataSize
    */
    virtual size_t getDataSize()
    {
        return g_nGroups * g_nVrtxPerFace * sizeof(double);
    }
    
    
    /*
        sideData
        
        Set face data for (side, angle) pair.
    */
    virtual void setSideData(UINT side, UINT angle, const char *data)
    {
        Mat2<double> localFaceData(g_nVrtxPerFace, g_nGroups, (double*)data);
        
        for (UINT group = 0; group < g_nGroups; group++) {
        for (UINT fvrtx = 0; fvrtx < g_nVrtxPerFace; fvrtx++) {
            c_psiBound(group, fvrtx, angle, side) = localFaceData(fvrtx, group);
        }}
    }
    
    /*
        getSideData
    */
    PsiBoundData getSideData()
    {return c_psiBound;};
    

    /*
        getPriority
        
        Return a priority for the cell/angle pair.
        Not needed for this class, so it is just set to a constant.
    */
    virtual UINT getPriority(UINT cell, UINT angle)
    {
        UNUSED_VARIABLE(cell);
        UNUSED_VARIABLE(angle);
        return 1;
    }
    
    
    /*
        update
        
        Updates psi for a given (cell, angle) pair.
    */
    virtual void update(UINT cell, UINT angle, 
                        UINT adjCellsSides[g_nFacePerCell], 
                        BoundaryType bdryType[g_nFacePerCell])
    {
        UNUSED_VARIABLE(adjCellsSides);
        UNUSED_VARIABLE(bdryType);
        
        Mat2<double> localSource(g_nVrtxPerCell, g_nGroups);
        Mat2<double> localPsi(g_nVrtxPerCell, g_nGroups);
        Mat3<double> localPsiBound(g_nVrtxPerFace, g_nFacePerCell, g_nGroups);
        
        
        // Populate localSource
        for (UINT group = 0; group < g_nGroups; group++) {
        for (UINT vrtx = 0; vrtx < g_nVrtxPerCell; vrtx++) {
            localSource(vrtx, group) = c_source(group, vrtx, angle, cell);
        }}
        
        
        // Populate localPsiBound
        Transport::populateLocalPsiBound(angle, cell, c_psi, c_psiBound, 
                                         localPsiBound);
        
        
        // Transport solve
        Transport::solve(cell, angle, c_sigmaTotal, 
                         localPsiBound, localSource, localPsi);
        
        
        // localPsi -> psi
        for (UINT group = 0; group < g_nGroups; group++) {
        for (UINT vrtx = 0; vrtx < g_nVrtxPerCell; vrtx++) {
            c_psi(group, vrtx, angle, cell) = localPsi(vrtx, group);
        }}
    }
    
private:
    PsiData &c_psi;
    PsiBoundData &c_psiBound;
    const PsiData &c_source;
    const double c_sigmaTotal;
    std::vector<Mat2<double>> c_localFaceData;
};

#endif

