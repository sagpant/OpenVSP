//
// This file is released under the terms of the NASA Open Source Agreement (NOSA)
// version 1.3 as detailed in the LICENSE file which accompanies this software.
//

// ParasiteDragMgr.cpp: ParasiteDrag Mgr Singleton.
//
//////////////////////////////////////////////////////////////////////

#include "Vehicle.h"
#include "VehicleMgr.h"
#include "ParmMgr.h"
#include "StlHelper.h"
#include "APIDefines.h"
#include "WingGeom.h"
#include "ResultsMgr.h"
#include "ParasiteDragMgr.h"

#include "eli/mutil/nls/newton_raphson_method.hpp"

#include <numeric>

//==== Constructor ====//
ParasiteDragMgrSingleton::ParasiteDragMgrSingleton() : ParmContainer()
{
    // Initial Values for certain variables
    SetDefaultStruct();
    m_Name = "ParasiteDragSettings";
    m_FileName = "ParasiteDragBuildUp.csv";
    string groupname = "ParasiteDrag";
    m_LamCfEqnName = "Blasius";
    m_TurbCfEqnName = "Blasius Power Law";
    m_RefGeomID = "";
    m_CurrentExcresIndex = -1;
    m_CompGeomResults = NULL;

    // ==== Parm Initialize and Description Setting ==== //
    // Reference Qualities Parms
    m_SortByFlag.Init("SortBy", groupname, this, PD_SORT_NONE, PD_SORT_NONE, PD_SORT_PERC_CD);
    m_SortByFlag.SetDescript("Flag to determine what geometries are sorted by");

    m_RefFlag.Init("RefFlag", groupname, this, MANUAL_REF, MANUAL_REF, COMPONENT_REF);
    m_RefFlag.SetDescript("Reference quantity flag");

    m_Sref.Init("Sref", groupname, this, 100.0, 0.0, 1e12);
    m_Sref.SetDescript("Reference area");

    m_LamCfEqnType.Init("LamCfEqnType", groupname, this, vsp::CF_LAM_BLASIUS, vsp::CF_LAM_BLASIUS, vsp::CF_LAM_BLASIUS_W_HEAT);
    m_LamCfEqnType.SetDescript("Laminar Cf Equation Choice");

    m_TurbCfEqnType.Init("TurbCfEqnType", groupname, this, vsp::CF_TURB_POWER_LAW_BLASIUS, vsp::CF_TURB_EXPLICIT_FIT_SPALDING,
        vsp::CF_TURB_HEATTRANSFER_WHITE_CHRISTOPH);
    m_TurbCfEqnType.SetDescript("Turbulent Cf Equation Choice");

    m_AltLengthUnit.Init("AltLengthUnit", groupname, this, vsp::PD_UNITS_IMPERIAL, vsp::PD_UNITS_IMPERIAL, vsp::PD_UNITS_METRIC);
    m_AltLengthUnit.SetDescript("Altitude Units");

    m_LengthUnit.Init("LengthUnit", groupname, this, vsp::LEN_FT, vsp::LEN_MM, vsp::LEN_UNITLESS);
    m_LengthUnit.SetDescript("Length Units");

    m_TempUnit.Init("TempUnit", groupname, this, vsp::TEMP_UNIT_F, vsp::TEMP_UNIT_K, vsp::TEMP_UNIT_R);
    m_TempUnit.SetDescript("Temperature Units");

    // Air Qualities Parms
    m_FreestreamType.Init("FreestreamType", groupname, this, vsp::ATMOS_TYPE_US_STANDARD_1976,
        vsp::ATMOS_TYPE_US_STANDARD_1976, vsp::ATMOS_TYPE_MANUAL_RE_L);
    m_FreestreamType.SetDescript("Assigns the desired inputs to describe the freestream properties");

    m_Mach.Init("Mach", groupname, this, 0.0, 0.0, 1000.0);
    m_Mach.SetDescript("Mach Number for Current Flight Condition");

    m_ReqL.Init("Re_L", groupname, this, 0.0, 0.0, 1e12);
    m_ReqL.SetDescript("Reynolds Number Per Unit Length");

    m_Temp.Init("Temp", groupname, this, 288.15, -459.67, 1e12);
    m_Temp.SetDescript("Temperature");

    m_Pres.Init("Pres", groupname, this, 2116.221, 1e-4, 1e12);
    m_Pres.SetDescript("Pressure");

    m_Rho.Init("Density", groupname, this, 0.07647, 1e-12, 1e12);
    m_Rho.SetDescript("Density");

    m_DynaVisc.Init("DynaVisc", groupname, this, 0.0, 1e-12, 1e12);
    m_DynaVisc.SetDescript("Dynamic Viscosity for Current Condition");

    m_SpecificHeatRatio.Init("SpecificHeatRatio", groupname, this, 1.4, -1, 1e3);
    m_SpecificHeatRatio.SetDescript("Specific Heat Ratio");

    //m_MediumType.Init("Medium", groupname, this, Atmosphere::MEDIUM_AIR, Atmosphere::MEDIUM_AIR, Atmosphere::MEDIUM_PURE_WATER);
    //m_MediumType.SetDescript("Wind Tunnel Medium");

    m_Vinf.Init("Vinf", groupname, this, 500.0, 0.0, 1e12);
    m_Vinf.SetDescript("Free Stream Velocity");

    m_VinfUnitType.Init("VinfUnitType", groupname, this, vsp::V_UNIT_FT_S, vsp::V_UNIT_FT_S, vsp::V_UNIT_KTAS);
    m_VinfUnitType.SetDescript("Units for Freestream Velocity");

    m_Hinf.Init("Alt", groupname, this, 20000.0, 0.0, 271823.3);
    m_Hinf.SetDescript("Physical Altitude from Sea Level");

    m_DeltaT.Init("DeltaTemp", groupname, this, 0.0, -1e12, 1e12);
    m_DeltaT.SetDescript("Delta Temperature from STP");

    // Excrescence Parm
    m_ExcresValue.Init("ExcresVal", groupname, this, 0.0, 0.0, 200);
    m_ExcresValue.SetDescript("Excrescence Value");

    m_ExcresType.Init("ExcresType", groupname, this, vsp::EXCRESCENCE_COUNT, vsp::EXCRESCENCE_COUNT, vsp::EXCRESCENCE_DRAGAREA);
    m_ExcresType.SetDescript("Excrescence Type");
}

void ParasiteDragMgrSingleton::Renew()
{
    m_TableRowVec.clear();
    m_ExcresRowVec.clear();

    m_DegenGeomVec.clear();
    m_CompGeomResults = NULL;

    SetDefaultStruct();

    m_FileName = "ParasiteDragBuildUp.csv";
    m_LamCfEqnName = "Blasius";
    m_TurbCfEqnName = "Blasius Power Law";
    m_RefGeomID = "";

    m_ExcresType = 0;
    m_ExcresValue = 0;

    m_CurrentExcresIndex = -1;
}

void ParasiteDragMgrSingleton::SetDefaultStruct()
{
    m_DefaultStruct.GeomID = "";
    m_DefaultStruct.SubSurfID = "";
    m_DefaultStruct.Label = "";
    m_DefaultStruct.Swet = -1;
    m_DefaultStruct.Lref = -1;
    m_DefaultStruct.Re = -1;
    m_DefaultStruct.Roughness = -1;
    m_DefaultStruct.TeTwRatio = -1;
    m_DefaultStruct.TawTwRatio = -1;
    m_DefaultStruct.PercLam = 0.0;
    m_DefaultStruct.Cf = -1;
    m_DefaultStruct.fineRat = -1;
    m_DefaultStruct.GeomShapeType = 0;
    m_DefaultStruct.FFEqnChoice = 0;
    m_DefaultStruct.FFEqnName = "";
    m_DefaultStruct.FF = -1;
    m_DefaultStruct.Q = 1;
    m_DefaultStruct.f = -1;
    m_DefaultStruct.CD = -1;
    m_DefaultStruct.PercTotalCd = -1;
    m_DefaultStruct.SurfNum = 0;
    m_DefaultStruct.GroupedAncestorGen = 0;
    m_DefaultStruct.ExpandedList = false;
}

void ParasiteDragMgrSingleton::ParmChanged(Parm* parm_ptr, int type)
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    if (veh)
    {
        veh->ParmChanged(parm_ptr, type);
    }
}

void ParasiteDragMgrSingleton::InitTableVec()
{
    m_TableRowVec.clear();
    for (int i = 0; i < m_RowSize; ++i)
    {
        m_TableRowVec.push_back(m_DefaultStruct);
    }
}

void ParasiteDragMgrSingleton::LoadMainTableUserInputs()
{
    Geom* geom;
    char str[256];
    Vehicle *veh = VehicleMgr.GetVehicle();

    for (int i = 0; i < m_PDGeomIDVec.size(); i++)
    {
        geom = veh->FindGeom(m_PDGeomIDVec[i]);
        if (geom)
        {
            for (int j = 0; j < geom->GetNumTotalSurfs(); j++)
            {
                // Custom Geom Check: if surf type is the same, apply same qualities
                if (j > 0 && geom->GetSurfPtr(j)->GetSurfType() == geom->GetSurfPtr(j - 1)->GetSurfType())
                {
                    geo_groupedAncestorGen.push_back(geom->m_GroupedAncestorGen());
                    geo_percLam.push_back(geo_percLam[geo_percLam.size() - 1]);
                    geo_ffIn.push_back(geo_ffIn[geo_ffIn.size() - 1]);
                    geo_Q.push_back(geo_Q[geo_Q.size() - 1]);
                    geo_Roughness.push_back(geo_Roughness[geo_Roughness.size() - 1]);
                    geo_TeTwRatio.push_back(geo_TeTwRatio[geo_TeTwRatio.size() - 1]);
                    geo_TawTwRatio.push_back(geo_TawTwRatio[geo_TawTwRatio.size() - 1]);
                    geo_surfNum.push_back(j);
                    geo_expandedList.push_back(false);
                    sprintf(str, "%s_%d", geom->GetName().c_str(), j);
                }
                else
                {
                    if (geom->GetType().m_Type == CUSTOM_GEOM_TYPE)
                    {
                        if (geom->GetSurfPtr(j)->GetSurfType() == vsp::NORMAL_SURF)
                        {
                            sprintf(str, "[B] %s", geom->GetName().c_str());
                        }
                        else
                        {
                            sprintf(str, "[W] %s", geom->GetName().c_str());
                        }
                        geo_surfNum.push_back(j);
                    }
                    else
                    {
                        sprintf(str, "%s", geom->GetName().c_str());
                        geo_surfNum.push_back(0);
                    }
                    geo_groupedAncestorGen.push_back(geom->m_GroupedAncestorGen());
                    geo_percLam.push_back(geom->m_PercLam());
                    geo_ffIn.push_back(geom->m_FFUser());
                    geo_Q.push_back(geom->m_Q());
                    geo_Roughness.push_back(geom->m_Roughness());
                    geo_TeTwRatio.push_back(geom->m_TeTwRatio());
                    geo_TawTwRatio.push_back(geom->m_TawTwRatio());
                    geo_expandedList.push_back(geom->m_ExpandedListFlag());
                }

                geo_shapeType.push_back(geom->GetSurfPtr(j)->GetSurfType()); // Form Factor Shape Type

                if (geom->GetSurfPtr(j)->GetSurfType() == vsp::NORMAL_SURF)
                {
                    geo_ffType.push_back(geom->m_FFBodyEqnType());
                }
                else
                {
                    geo_ffType.push_back(geom->m_FFWingEqnType());
                }
                geo_geomID.push_back(geom->GetID());
                geo_subsurfID.push_back("");

                // Assign Label to Geom
                geo_label.push_back(str);
            }

            // Sub Surfaces
            for (int j = 0; j < geom->GetSubSurfVec().size(); j++)
            {
                for (int k = 0; k < geom->GetNumTotalSurfs(); ++k)
                {
                    geo_groupedAncestorGen.push_back(-1);
                    geo_percLam.push_back(geo_percLam[geo_percLam.size() - 1]); //TODO: Add Perc Lam to SubSurf
                    geo_ffIn.push_back(geo_ffIn[geo_ffIn.size() - 1]);
                    geo_Q.push_back(geo_Q[geo_Q.size() - 1]); // TODO: Add Q to SubSurf
                    geo_Roughness.push_back(geo_Roughness[geo_Roughness.size() - 1]); //TODO: Add Roughness to SubSurf
                    geo_TeTwRatio.push_back(geo_TeTwRatio[geo_TeTwRatio.size() - 1]);
                    geo_TawTwRatio.push_back(geo_TawTwRatio[geo_TawTwRatio.size() - 1]);
                    geo_surfNum.push_back(k);
                    geo_expandedList.push_back(false);

                    geo_shapeType.push_back(geom->GetSurfPtr(k)->GetSurfType()); // Form Factor Shape Type

                    if (geom->GetSurfPtr(k)->GetSurfType() == vsp::NORMAL_SURF)
                    {
                        geo_ffType.push_back(geom->m_FFBodyEqnType());
                    }
                    else
                    {
                        geo_ffType.push_back(geom->m_FFWingEqnType());
                    }
                    geo_geomID.push_back(geom->GetID());
                    geo_subsurfID.push_back(geom->GetSubSurf(j)->GetID());
                    sprintf(str, "[ss] %s_%i", geom->GetSubSurfVec()[j]->GetName().c_str(), k);

                    // Assign Label to Geom
                    geo_label.push_back(str);
                }
            }
        }
    }
}

void ParasiteDragMgrSingleton::SetupFullCalculation()
{
    Vehicle* veh = VehicleMgr.GetVehicle();
    if (veh)
    {
        veh->ClearDegenGeom();
        ResultsMgr.DeleteResult(ResultsMgr.FindResultsID("Comp_Geom"));
        ClearInputVectors();
        ClearOutputVectors();

        veh->CreateDegenGeom(m_SetChoice());
        string meshID = veh->CompGeomAndFlatten(m_SetChoice(), 0);
        veh->DeleteGeom(meshID);
        veh->ShowOnlySet(m_SetChoice());

        // First Assignment of DegenGeomVec, Will Carry Through to Rest of Calculate_X
        m_DegenGeomVec = veh->GetDegenGeomVec();

        // First Assignment of CompGeon, Will Carry Through to Rest of Calculate_X
        m_CompGeomResults = ResultsMgr.FindResults("Comp_Geom");
    }
}

int ParasiteDragMgrSingleton::CalcRowSize()
{
    Vehicle* veh = VehicleMgr.GetVehicle();
    if (!veh)
    {
        return 0;
    }

    m_RowSize = 0; // Reset every call
    for (int i = 0; i < m_PDGeomIDVec.size(); i++)
    {
        Geom* geom = veh->FindGeom(m_PDGeomIDVec[i]);
        if (geom)
        {
            m_RowSize += geom->GetNumTotalSurfs();
            for (size_t j = 0; j < geom->GetSubSurfVec().size(); ++j)
            {
                for (size_t k = 0; k < geom->GetNumSymmCopies(); ++k)
                {
                    ++m_RowSize;
                }
            }
        }
    }
    return m_RowSize;
}

void ParasiteDragMgrSingleton::Calculate_Swet()
{
    Vehicle* veh = VehicleMgr.GetVehicle();
    char str[256];
    string newstr;
    int searchIndex;

    if (!veh)
    {
        return;
    }

    int iSurf = 0;
    for (int i = 0; i < m_RowSize; ++i)
    {
        if (!m_DegenGeomVec.empty())
        { // If DegenGeom Exists Pull Swet
            vector < string > tagnamevec = m_CompGeomResults->Find("Tag_Name").GetStringData();
            if (geo_subsurfID[i].compare("") == 0)
            {
                sprintf(str, "%s%i", veh->FindGeom(geo_geomID[i])->GetName().c_str(), geo_surfNum[i]);
                newstr = str;
                searchIndex = vector_find_val(tagnamevec, newstr);
                geo_swet.push_back(m_CompGeomResults->Find("Tag_Wet_Area").GetDouble(searchIndex));
                ++iSurf;
            }
            else
            {
                sprintf(str, "%s%i,%s", veh->FindGeom(geo_geomID[i])->GetName().c_str(), geo_surfNum[i],
                    veh->FindGeom(geo_geomID[i])->GetSubSurf(geo_subsurfID[i])->GetName().c_str());
                newstr = str;
                searchIndex = vector_find_val(tagnamevec, newstr);
                geo_swet.push_back(m_CompGeomResults->Find("Tag_Wet_Area").GetDouble(searchIndex));
            }
        }
        else
        { // Else Push Back Default Val
            geo_swet.push_back(-1);
        }
    }

    UpdateWettedAreaTotals();
}

void ParasiteDragMgrSingleton::Calculate_Lref()
{
    // Initialize Variables
    int iSurf = 0;

    for (int i = 0; i < m_RowSize; ++i)
    {
        if (!m_DegenGeomVec.empty())
        { // If DegenGeom Exists Calculate Lref
            if (geo_subsurfID[i].compare("") == 0)
            {
                if (m_DegenGeomVec[iSurf].getType() != DegenGeom::DISK_TYPE)
                {
                    if (m_DegenGeomVec[iSurf].getType() == DegenGeom::SURFACE_TYPE)
                    {
                        CalcReferenceChord(iSurf);
                    }
                    else if (m_DegenGeomVec[iSurf].getType() == DegenGeom::BODY_TYPE)
                    {
                        CalcReferenceBodyLength(iSurf);
                    }
                    ++iSurf;
                }
                else
                {
                    --i;
                    ++iSurf;
                }
            }
            else
            {
                geo_lref.push_back(geo_lref[geo_lref.size() - 1]);
            }
        }
        else
        { // Else Push Back Default Val
            geo_lref.push_back(-1);
        }
    }
}

// Use Bounding Box to approximate x directional length
void ParasiteDragMgrSingleton::CalcReferenceBodyLength(int index)
{
    // TODO: Improve Reference Length Calculations
    double delta_x, delta_y, delta_z, lref;
    vector <DegenStick> m_DegenStick = m_DegenGeomVec[index].getDegenSticks();
    delta_x = abs(m_DegenStick[0].xle.front().x() - m_DegenStick[0].xle.back().x());
    delta_y = abs(m_DegenStick[0].xle.front().y() - m_DegenStick[0].xle.back().y());
    delta_z = abs(m_DegenStick[0].xle.front().z() - m_DegenStick[0].xle.back().z());
    lref = sqrt( pow(delta_x,2.0) + pow(delta_y,2.0) + pow(delta_z,2.0) );

    if (lref <= 1e-6) // Is it basically zero?
    {
        // Attempt to use chord from DegenGeom
        CalcReferenceChord(index);
    }

    // If STILL 0
    if (lref <= 1e-6)
    {
        geo_lref.push_back(1.0);
    }
    else
    {
        geo_lref.push_back(lref);
    }
}

// Use weighted average to approximate reference chord
void ParasiteDragMgrSingleton::CalcReferenceChord(int index)
{
    // TODO: Improve Reference Length Calculations
    vector <DegenStick> m_DegenStick = m_DegenGeomVec[index].getDegenSticks();
    double secArea, totalArea = 0, weightedChordSum = 0;
    double delta_x, delta_y, delta_z, section_span;
    for (size_t j = 0; j <= m_DegenStick[0].areaTop.size() - 1; ++j)
    {
        delta_x = abs(m_DegenStick[0].xle[j].x() - m_DegenStick[0].xle[j + 1].x());
        delta_y = abs(m_DegenStick[0].xle[j].y() - m_DegenStick[0].xle[j + 1].y());
        delta_z = abs(m_DegenStick[0].xle[j].z() - m_DegenStick[0].xle[j + 1].z());
        section_span = sqrt( pow(delta_x,2.0) + pow(delta_y,2.0) + pow(delta_z,2.0) );
        secArea = section_span* (0.5 * (m_DegenStick[0].chord[j] + m_DegenStick[0].chord[j + 1]));

        totalArea += secArea;

        weightedChordSum += m_DegenStick[0].chord[j] * secArea;
    }
    double lref = weightedChordSum / totalArea;

    if (lref <= 1e-6) // Is it basically zero?
    {
        // Attempt to use chord from DegenGeom
        CalcReferenceBodyLength(index);
    }

    // If STILL 0
    if (lref <= 1e-6)
    {
        geo_lref.push_back(1.0);
    }
    else
    {
        geo_lref.push_back(lref);
    }
}

void ParasiteDragMgrSingleton::Calculate_Re()
{
    for (int i = 0; i < m_RowSize; ++i)
    {
        if (!m_DegenGeomVec.empty())
        { // If DegenGeom Exists Calculate Re
            if (geo_subsurfID[i].compare("") == 0)
            {
                ReynoldsNumCalc(i);
            }
            else
            {
                geo_Re.push_back(geo_Re[geo_Re.size() - 1]);
            }

        }
        else
        { // Else Push Back Default Val
            geo_Re.push_back(-1);
        }
    }

    CalcRePowerDivisor();
}

void ParasiteDragMgrSingleton::CalcRePowerDivisor()
{
    if (!geo_Re.empty())
    {
        vector<double>::const_iterator it = max_element(geo_Re.begin(), geo_Re.end());
        m_ReynoldsPowerDivisor = mag(*it);
    }
    else
    {
        m_ReynoldsPowerDivisor = 1;
    }
}

void ParasiteDragMgrSingleton::ReynoldsNumCalc(int index)
{
    double vinf, lref;
    if (m_FreestreamType() != vsp::ATMOS_TYPE_MANUAL_RE_L)
    {
        vinf = m_Vinf();
        lref = geo_lref[index];

        if (m_AltLengthUnit() == vsp::PD_UNITS_IMPERIAL)
        {
            vinf = ConvertVelocity(vinf, m_VinfUnitType.Get(), vsp::V_UNIT_FT_S);
            lref = ConvertLength(lref, m_LengthUnit(), vsp::LEN_FT);
        }
        else if (m_AltLengthUnit() == vsp::PD_UNITS_METRIC)
        {
            vinf = ConvertVelocity(vinf, m_VinfUnitType.Get(), vsp::V_UNIT_M_S);
            lref = ConvertLength(lref, m_LengthUnit(), vsp::LEN_M);
        }

        geo_Re.push_back((vinf * lref) / m_KineVisc());
    }
    else
    { // Any other freestream definition type
        geo_Re.push_back(m_ReqL() * geo_lref[index]);
    }
}

void ParasiteDragMgrSingleton::Calculate_Cf()
{
    // Initialize Variables
    double lref, rho, kineVisc, vinf;

    for (int i = 0; i < m_RowSize; ++i)
    {
        if (!m_DegenGeomVec.empty())
        { // If DegenGeom Exists Calculate Cf
            if (geo_subsurfID[i].compare("") == 0)
            {
                vinf = ConvertVelocity(m_Vinf(), m_VinfUnitType.Get(), vsp::V_UNIT_M_S);
                rho = ConvertDensity(m_Atmos.GetDensity(), m_AltLengthUnit(), vsp::RHO_UNIT_KG_M3); // lb/ft3 to kg/m3
                lref = ConvertLength(geo_lref[i], m_LengthUnit(), vsp::LEN_M);
                kineVisc = m_Atmos.GetDynaVisc() / rho;

                if (geo_percLam[i] == 0 || geo_percLam[i] == -1)
                { // Assume full turbulence
                    geo_cf.push_back(CalcTurbCf(geo_Re[i], geo_lref[i], m_TurbCfEqnType(),
                        m_SpecificHeatRatio(), geo_Roughness[i], geo_TawTwRatio[i], geo_TeTwRatio[i]));
                }
                else
                { // Not full turbulence 
                    CalcPartialTurbulence(i, lref, vinf, kineVisc);
                }
            }
            else
            {
                geo_cf.push_back(geo_cf[geo_cf.size() - 1]);
            }
        }
        else
        { // Else push back default value
            geo_cf.push_back(-1);
        }
    }
}

void ParasiteDragMgrSingleton::CalcPartialTurbulence(int i, double lref, double vinf, double kineVisc)
{
    if (geo_Re[i] != 0)
    { // Prevent dividing by 0 in some equations
        double LamPerc = (geo_percLam[i] / 100);
        double CffullTurb = CalcTurbCf(geo_Re[i], geo_lref[i], m_TurbCfEqnType(),
            m_SpecificHeatRatio(), geo_Roughness[i], geo_TawTwRatio[i], geo_TeTwRatio[i]);
        double CffullLam = CalcLamCf(geo_Re[i], m_LamCfEqnType.Get());

        double LamPercRefLen = LamPerc * lref;

        double ReLam = (vinf * LamPercRefLen) / kineVisc;

        double CfpartLam = CalcLamCf(ReLam, m_LamCfEqnType());
        double CfpartTurb = CalcTurbCf(ReLam, geo_lref[i], m_TurbCfEqnType(),
            m_SpecificHeatRatio(), geo_Roughness[i], geo_TawTwRatio[i], geo_TeTwRatio[i]);

        m_TurbCfEqnName = AssignTurbCfEqnName(m_TurbCfEqnType());
        m_LamCfEqnName = AssignLamCfEqnName(m_LamCfEqnType());

        geo_cf.push_back(CffullTurb - (CfpartTurb * LamPerc) +
            (CfpartLam * LamPerc));
    }
    else
    {
        geo_cf.push_back(0);
    }
}

void ParasiteDragMgrSingleton::Calculate_fineRat()
{
    // Initialize Variables
    vector<double>::const_iterator it;
    double max_xsecarea, dia;
    int iSurf = 0;

    for (int i = 0; i < m_RowSize; ++i)
    {
        if (!m_DegenGeomVec.empty())
        { // If DegenGeom Exists Calculate Fineness Ratio
            if (geo_subsurfID[i].compare("") == 0)
            {
                // Grab Degen Sticks for Appropriate Geom
                vector <DegenStick> degenSticks = m_DegenGeomVec[iSurf].getDegenSticks();

                if (m_DegenGeomVec[iSurf].getType() != DegenGeom::DISK_TYPE)
                {
                    if (m_DegenGeomVec[iSurf].getType() == DegenGeom::SURFACE_TYPE)
                    { // Wing Type
                        it = max_element(degenSticks[0].toc.begin(), degenSticks[0].toc.end());
                        geo_fineRat.push_back(*it);
                    }
                    else if (m_DegenGeomVec[iSurf].getType() == DegenGeom::BODY_TYPE)
                    {
                        it = max_element(degenSticks[0].sectarea.begin(), degenSticks[0].sectarea.end());
                        max_xsecarea = *it;

                        // Use Max X-Sectional Area to find "Nominal" Diameter
                        dia = 2 * sqrt((max_xsecarea / (PI)));

                        geo_fineRat.push_back(dia / geo_lref[i]);
                    }
                    ++iSurf;
                }
                else
                {
                    --i;
                    ++iSurf;
                }
            }
            else
            {
                geo_fineRat.push_back(geo_fineRat[geo_fineRat.size() - 1]);
            }
        }
        else
        { // Else Push Back Default Val
            geo_fineRat.push_back(-1);
        }
    }
}

void ParasiteDragMgrSingleton::Calculate_FF()
{
    // Initialize Variables
    vector<double>::const_iterator it;
    double toc;
    double fin_rat, longF, FR, Area;
    vector <double> hVec, wVec;
    int iSurf = 0;

    for (int i = 0; i < m_RowSize; ++i)
    {
        if (!m_DegenGeomVec.empty())
        { // If DegenGeom Exists Calculate Form Factor
            if (geo_subsurfID[i].compare("") == 0)
            {
                // Grab Degen Sticks for Appropriate Geom
                vector <DegenStick> degenSticks = m_DegenGeomVec[iSurf].getDegenSticks();

                if (m_DegenGeomVec[iSurf].getType() == DegenGeom::SURFACE_TYPE)
                { // Wing Type

                    toc = geo_fineRat[i];

                    Calculate_AvgSweep(degenSticks);

                    geo_ffOut.push_back(CalcFFWing(toc, geo_ffType[i], geo_percLam[i], m_Sweep25, m_Sweep50));
                    if (geo_ffType[i] == vsp::FF_W_JENKINSON_TAIL)
                    {
                        geo_Q[i] = 1.2;
                    }
                    geo_ffName.push_back(AssignFFWingEqnName(geo_ffType[i]));
                }
                else if (m_DegenGeomVec[iSurf].getType() == DegenGeom::BODY_TYPE)
                {
                    // Get Fine Rat
                    fin_rat = geo_fineRat[i];

                    // Invert Fineness Ratio
                    longF = pow(geo_fineRat[i], -1);

                    // Max Cross Sectional Area
                    Area = *max_element(degenSticks[0].areaTop.begin(), degenSticks[0].areaTop.end());

                    // FR used by Schemensky
                    FR = geo_lref[i]/ sqrt(Area);

                    geo_ffOut.push_back(CalcFFBody(longF, FR, geo_ffType[i], geo_lref[i], Area));
                    geo_ffName.push_back(AssignFFBodyEqnName(geo_ffType[i]));
                }
                else
                {
                    --i;
                }
                ++iSurf;
            }
            else
            {
                geo_ffOut.push_back(geo_ffOut[geo_ffOut.size() - 1]);
                geo_ffName.push_back(geo_ffName[geo_ffName.size() - 1]);
            }
        }
        // Else Push Back Default Val
        else
        {
            geo_ffOut.push_back(-1);
            geo_ffName.push_back("");
        }
    }
}

void ParasiteDragMgrSingleton::Calculate_AvgSweep(vector<DegenStick> degenSticks)
{
    // Find Quarter Chord Using Derived Eqn from Geometry
    double width, secSweep25, secSweep50, secArea, weighted25Sum = 0, weighted50Sum = 0, totalArea = 0;
    for (int j = 0; j < degenSticks[0].areaTop.size(); j++)
    {
        width = degenSticks[0].areaTop[j] /
            ((degenSticks[0].perimTop[j] + degenSticks[0].perimTop[j + 1.0]) / 2.0);

        // Section Quarter Chord Sweep
        secSweep25 = atan(tan(degenSticks[0].sweeple[j] * PI / 180.0) +
            (0.25 * ((degenSticks[0].chord[j] - degenSticks[0].chord[j + 1.0]) / width))) *
            180.0 / PI;

        // Section Half Chord Sweep
        secSweep50 = atan(tan(degenSticks[0].sweeple[j] * PI / 180.0) +
            (0.50 * ((degenSticks[0].chord[j] - degenSticks[0].chord[j + 1.0]) / width))) *
            180.0 / PI;

        // Section Area
        secArea = degenSticks[0].chord[j] * width;

        // Add Weighted Value to Weighted Sum
        weighted25Sum += secArea*secSweep25;
        weighted50Sum += secArea*secSweep50;

        // Continue to sum up Total Area
        totalArea += secArea;
    }

    // Calculate Sweep @ c/4 & c/2
    m_Sweep25 = weighted25Sum / totalArea * PI / 180.0; // Into Radians
    m_Sweep50 = weighted50Sum / totalArea * PI / 180.0; // Into Radians
}

void ParasiteDragMgrSingleton::Calculate_f()
{
    Vehicle* veh = VehicleMgr.GetVehicle();
    // Initialize Variables
    double Q, ff;

    for (int i = 0; i < m_RowSize; ++i)
    {
        // If no value input as Q, use 1
        if (geo_Q[i] != -1)
        {
            Q = geo_Q[i];
        }
        else
        {
            Q = 1;
        }

        // If no value input as FF, use calculated
        if (geo_ffIn[i] != -1)
        {
            ff = geo_ffIn[i];
        }
        else
        {
            ff = geo_ffOut[i];
        }

        if (IsNotZeroLineItem(i))
        {
            if (!m_DegenGeomVec.empty())
            { // If DegenGeom Exists Calculate f
                geo_f.push_back(geo_swet[i] * Q * geo_cf[i] * ff);
            }
            else
            { // Else Push Back Default Val
                geo_f.push_back(-1);
            }
        }
        else
        {
            if (!m_DegenGeomVec.empty())
            {
                geo_f.push_back(0.0);
            }
            else
            {
                geo_f.push_back(-1);
            }
        }
    }
}

void ParasiteDragMgrSingleton::Calculate_Cd()
{
    Vehicle* veh = VehicleMgr.GetVehicle();
    int iSubSurf = 0;

    for (int i = 0; i < m_RowSize; ++i)
    {
        if (IsNotZeroLineItem(i))
        {
            if (!m_DegenGeomVec.empty())
            { // If DegenGeom Exists Calculate CD
                if (!isnan(geo_f[i]))
                {
                    geo_Cd.push_back(geo_f[i] / m_Sref.Get());
                }
                else
                {
                    geo_Cd.push_back(0.0);
                }
            }
            else
            { // Else Push Back Default Val
                geo_Cd.push_back(-1);
            }
        }
        else
        {
            if (!m_DegenGeomVec.empty())
            {
                geo_Cd.push_back(0.0);
            }
            else
            {
                geo_Cd.push_back(-1);
            }
        }
    }
}

void ParasiteDragMgrSingleton::Calculate_ALL()
{
    ClearOutputVectors();
    ClearInputVectors();
    LoadMainTableUserInputs(); // Load User Input Values

    // Calculate All Necessary Values
    Calculate_Swet();
    Calculate_Lref();
    Calculate_Re();
    Calculate_Cf();
    Calculate_fineRat();
    Calculate_FF();
    OverwritePropertiesFromAncestorGeom();
    Calculate_f();
    Calculate_Cd();

    UpdateExcres();
    UpdatePercentageCD();

    InitTableVec(); // Initialize Map Size

    ParasiteDragTableRow tempStruct = m_DefaultStruct;
    for (int i = 0; i < m_RowSize; i++)
    {
        tempStruct.GroupedAncestorGen = geo_groupedAncestorGen[i];
        tempStruct.GeomID = geo_geomID[i];
        tempStruct.SubSurfID = geo_subsurfID[i];
        tempStruct.Label = geo_label[i];
        tempStruct.Swet = geo_swet[i];
        tempStruct.Lref = geo_lref[i];
        tempStruct.Re = geo_Re[i];
        tempStruct.PercLam = geo_percLam[i];
        tempStruct.Cf = geo_cf[i];
        tempStruct.fineRat = geo_fineRat[i];
        tempStruct.FFEqnChoice = geo_ffType[i];
        tempStruct.FFEqnName = geo_ffName[i];
        tempStruct.Roughness = geo_Roughness[i];
        tempStruct.TeTwRatio = geo_TeTwRatio[i];
        tempStruct.TawTwRatio = geo_TawTwRatio[i];
        tempStruct.GeomShapeType = geo_shapeType[i];
        tempStruct.SurfNum = geo_surfNum[i];
        if (geo_ffType[i] == vsp::FF_B_MANUAL || geo_ffType[i] == vsp::FF_W_MANUAL)
        {
            tempStruct.FF = geo_ffIn[i];
        }
        else
        {
            tempStruct.FF = geo_ffOut[i];
        }
        tempStruct.Q = geo_Q[i];
        tempStruct.f = geo_f[i];
        tempStruct.CD = geo_Cd[i];
        tempStruct.PercTotalCd = geo_percTotalCd[i];
        tempStruct.SurfNum = geo_surfNum[i];
        tempStruct.ExpandedList = geo_expandedList[i];

        m_TableRowVec[i] = tempStruct;
    }
}

void ParasiteDragMgrSingleton::OverwritePropertiesFromAncestorGeom()
{
    Vehicle* veh = VehicleMgr.GetVehicle();
    for (size_t i = 0; i < m_RowSize; ++i)
    {
        if (geo_groupedAncestorGen[i] > 0)
        {
            for (size_t j = 0; j < m_RowSize; ++j)
            {
                if (geo_geomID[j].compare(veh->FindGeom(geo_geomID[i])->GetAncestorID(geo_groupedAncestorGen[i])) == 0 &&
                    geo_surfNum[j] == 0)
                {
                    geo_lref[i] = geo_lref[j];
                    geo_Re[i] = geo_Re[j];
                    geo_fineRat[i] = geo_fineRat[j];
                    geo_ffOut[i] = geo_ffOut[j];
                    geo_ffType[i] = geo_ffType[j];
                    geo_percLam[i] = geo_percLam[j];
                    geo_Q[i] = geo_Q[j];
                    geo_cf[i] = geo_cf[j];
                }
            }
        }
    }
}

// ================================== //
// ====== Iterative Functions ======= //
// ================================== //

struct Schoenherr_functor
{
    double operator()(const double &Cf) const
    {
        return (0.242 / (sqrt(Cf) * log10(Re * Cf))) - 1.0;
    }
    double Re;
};

struct Schoenherr_p_functor
{
    double operator()(const double &Cf) const
    {
        return ((-0.278613 * log(Cf * Re)) - 0.557226) /
            (pow(Cf, 1.5) * pow(log(Re * Cf), 2.0));
    }
    double Re;
};

struct Karman_functor
{
    double operator()(const double &Cf) const
    {
        return ((4.15 * log10(Re * Cf) + 1.70) * sqrt(Cf)) - 1.0;
    }
    double Re;
};

struct Karman_p_functor
{
    double operator()(const double &Cf) const
    {
        return (0.901161 * log(Re * Cf) + 2.65232) / sqrt(Cf);
    }
    double Re;
};

struct Karman_Schoenherr_functor
{
    double operator()(const double &Cf) const
    {
        return ((4.13 * log10(Re * Cf)) * sqrt(Cf)) - 1.0;
    }
    double Re;
};

struct Karman_Schoenherr_p_functor
{
    double operator()(const double &Cf) const
    {
        return (0.896818 * log(Re * Cf) + 1.79364) / sqrt(Cf);
    }
    double Re;
};

// ================================== //
// ================================== //
// ================================== //

double ParasiteDragMgrSingleton::CalcTurbCf(double ReyIn, double ref_leng, int cf_case,
    double roughness_h = 0, double gamma = 1.4, double taw_tw_ratio = 0, double te_tw_ratio = 0)
{
    double CfOut, CfGuess, f, heightRatio, multiBy;
    double r = 0.89; // Recovery Factor
    double n = 0.67; // Viscosity Power-Law Exponent

    eli::mutil::nls::newton_raphson_method < double > nrm;

    if (m_LengthUnit.Get() == vsp::LEN_FT)
    {
        multiBy = 12.0;
    }
    else if (m_LengthUnit.Get() == vsp::LEN_M)
    {
        multiBy = 39.3701;
    }

    switch (cf_case)
    {
    case vsp::CF_TURB_WHITE_CHRISTOPH_COMPRESSIBLE:
        CfOut = 0.42 / pow(log(0.056 * ReyIn), 2.0);
        break;

    case vsp::CF_TURB_SCHLICHTING_PRANDTL:
        CfOut = 1 / pow((2 * log10(ReyIn) - 0.65), 2.3);
        break;

    case vsp::CF_TURB_SCHLICHTING_COMPRESSIBLE:
        CfOut = 0.455 / pow(log10(ReyIn), 2.58);
        break;

    case vsp::CF_TURB_SCHLICHTING_INCOMPRESSIBLE:
        CfOut = 0.472 / pow(log10(ReyIn), 2.5);
        break;

    case vsp::CF_TURB_SCHULTZ_GRUNOW_SCHOENHERR:
        CfOut = 0.427 / pow((log10(ReyIn) - 0.407), 2.64);
        break;

    case vsp::CF_TURB_SCHULTZ_GRUNOW_HIGH_RE:
        CfOut = 0.37 / pow(log10(ReyIn), 2.584);
        break;

    case vsp::CF_TURB_POWER_LAW_BLASIUS:
        CfOut = 0.0592 / pow(ReyIn, 0.2);
        break;

    case vsp::CF_TURB_POWER_LAW_PRANDTL_LOW_RE:
        CfOut = 0.074 / pow(ReyIn, 0.2);
        break;

    case vsp::CF_TURB_POWER_LAW_PRANDTL_MEDIUM_RE:
        CfOut = 0.027 / pow(ReyIn, 1.0 / 7.0);
        break;

    case vsp::CF_TURB_POWER_LAW_PRANDTL_HIGH_RE:
        CfOut = 0.058 / pow(ReyIn, 0.2);
        break;

    case vsp::CF_TURB_EXPLICIT_FIT_SPALDING:
        CfOut = 0.455 / pow(log(0.06 * ReyIn), 2.0);
        break;

    case vsp::CF_TURB_EXPLICIT_FIT_SPALDING_CHI:
        CfOut = 0.225 / pow(log10(ReyIn), 2.32);
        break;

    case vsp::CF_TURB_EXPLICIT_FIT_SCHOENHERR:
        CfOut = pow((1.0 / ((3.46* log10(ReyIn)) - 5.6)), 2.0);
        break;

    case vsp::CF_TURB_IMPLICIT_SCHOENHERR:
        Schoenherr_functor sfun;
        sfun.Re = ReyIn;
        Schoenherr_p_functor sfunprm;
        sfunprm.Re = ReyIn;

        CfGuess = pow((1.0 / ((3.46* log10(ReyIn)) - 5.6)), 2.0);

        nrm.set_initial_guess(CfGuess);
        nrm.find_root(CfOut, sfun, sfunprm, 0.0);
        break;

    case vsp::CF_TURB_IMPLICIT_KARMAN:
        Karman_functor kfun;
        kfun.Re = ReyIn;
        Karman_p_functor kfunprm;
        kfunprm.Re = ReyIn;

        CfGuess = 0.455 / pow(log10(ReyIn), 2.58);

        nrm.set_initial_guess(CfGuess);
        nrm.find_root(CfOut, kfun, kfunprm, 0.0);
        break;

    case vsp::CF_TURB_IMPLICIT_KARMAN_SCHOENHERR:
        Karman_Schoenherr_functor ksfun;
        ksfun.Re = ReyIn;
        Karman_Schoenherr_p_functor ksfunprm;
        ksfunprm.Re = ReyIn;

        CfGuess = pow((1.0 / ((3.46* log10(ReyIn)) - 5.6)), 2.0);

        nrm.set_initial_guess(CfGuess);
        nrm.find_root(CfOut, ksfun, ksfunprm, 0.0);
        break;

    case vsp::CF_TURB_ROUGHNESS_WHITE:
        heightRatio = ref_leng / roughness_h;
        CfOut = pow((1.4 + (3.7 * log10(heightRatio))), -2.0);
        break;

    case vsp::CF_TURB_ROUGHNESS_SCHLICHTING_LOCAL:
        heightRatio = ref_leng / roughness_h;
        CfOut = pow((1.4 + (3.7 * log10(heightRatio))), -2.0);
        break;

    case vsp::CF_TURB_ROUGHNESS_SCHLICHTING_AVG:
        heightRatio = ref_leng / (roughness_h * multiBy);
        CfOut = pow((1.89 + (1.62 * log10(heightRatio))), -2.5);
        break;

    case vsp::CF_TURB_ROUGHNESS_SCHLICHTING_AVG_FLOW_CORRECTION:
        heightRatio = ref_leng / (roughness_h * multiBy);
        CfOut = pow((1.89 + (1.62 * log10(heightRatio))), -2.5) /
            (pow((1.0 + ((gamma - 1.0) / 2.0) * m_Mach()), 0.467));
        break;

    case vsp::CF_TURB_HEATTRANSFER_WHITE_CHRISTOPH:
        f = (1 + (0.22 * r * (((roughness_h * multiBy) - 1.0) / 2.0) *
            m_Mach() * m_Mach() * te_tw_ratio)) /
            (1 + (0.3 * (taw_tw_ratio - 1.0)));

        CfOut = (0.451 * f * f * te_tw_ratio) /
            (log(0.056 * f * pow(te_tw_ratio, 1.0 + n) * ReyIn));

        break;

    default:
        CfOut = 0;
        break;
    }

    return CfOut;
}

double ParasiteDragMgrSingleton::CalcLamCf(double ReyIn, int cf_case)
{
    double CfOut;

    switch (cf_case)
    {
    case vsp::CF_LAM_BLASIUS:
        CfOut = 1.32824 / pow(ReyIn, 0.5);
        break;

    case vsp::CF_LAM_BLASIUS_W_HEAT:
        CfOut = 0;
        break;

    default:
        CfOut = 0;
    }

    return CfOut;
}

string ParasiteDragMgrSingleton::AssignTurbCfEqnName(int cf_case)
{
    string eqn_name;
    switch (cf_case)
    {
    case vsp::CF_TURB_WHITE_CHRISTOPH_COMPRESSIBLE:
        eqn_name = "Compressible White-Christoph";
        break;

    case vsp::CF_TURB_SCHLICHTING_PRANDTL:
        eqn_name = "Schlichting-Prandtl";
        break;

    case vsp::CF_TURB_SCHLICHTING_COMPRESSIBLE:
        eqn_name = "Compressible Schlichting";
        break;

    case vsp::CF_TURB_SCHLICHTING_INCOMPRESSIBLE:
        eqn_name = "Incompressible Schlichting";
        break;

    case vsp::CF_TURB_SCHULTZ_GRUNOW_SCHOENHERR:
        eqn_name = "Schultz-Grunow Schoenherr";
        break;

    case vsp::CF_TURB_SCHULTZ_GRUNOW_HIGH_RE:
        eqn_name = "High Reynolds Number Schultz-Grunow";
        break;

    case vsp::CF_TURB_POWER_LAW_BLASIUS:
        eqn_name = "Blasius Power Law";
        break;

    case vsp::CF_TURB_POWER_LAW_PRANDTL_LOW_RE:
        eqn_name = "Low Reynolds Number Prandtl Power Law";
        break;

    case vsp::CF_TURB_POWER_LAW_PRANDTL_MEDIUM_RE:
        eqn_name = "Medium Reynolds Number Prandtl Power Law";
        break;

    case vsp::CF_TURB_POWER_LAW_PRANDTL_HIGH_RE:
        eqn_name = "High Reynolds Number Prandtl Power Law";
        break;

    case vsp::CF_TURB_EXPLICIT_FIT_SPALDING:
        eqn_name = "Spalding Explicit Empirical Fit";
        break;

    case vsp::CF_TURB_EXPLICIT_FIT_SPALDING_CHI:
        eqn_name = "Spalding-Chi Explicit Empirical Fit";
        break;

    case vsp::CF_TURB_EXPLICIT_FIT_SCHOENHERR:
        eqn_name = "Schoenherr Explicit Empirical Fit";
        break;

    case vsp::CF_TURB_IMPLICIT_SCHOENHERR:
        eqn_name = "Schoenherr Implicit";
        break;

    case vsp::CF_TURB_IMPLICIT_KARMAN:
        eqn_name = "Von Karman Implicit";
        break;

    case vsp::CF_TURB_IMPLICIT_KARMAN_SCHOENHERR:
        eqn_name = "Karman-Schoenherr Implicit";
        break;

    case vsp::CF_TURB_ROUGHNESS_WHITE:
        eqn_name = "White Roughness";
        break;

    case vsp::CF_TURB_ROUGHNESS_SCHLICHTING_LOCAL:
        eqn_name = "Schlichting Local Roughness";
        break;

    case vsp::CF_TURB_ROUGHNESS_SCHLICHTING_AVG:
        eqn_name = "Schlichting Avg Roughness";
        break;

    case vsp::CF_TURB_ROUGHNESS_SCHLICHTING_AVG_FLOW_CORRECTION:
        m_TurbCfEqnName = "Schlichting Avg Roughness w Flow Correctioin";
        break;

    case vsp::CF_TURB_HEATTRANSFER_WHITE_CHRISTOPH:
        eqn_name = "White-Christoph w Heat Transfer";
        break;

    default:
        eqn_name = "ERROR";
        break;
    }
    return eqn_name;
}

string ParasiteDragMgrSingleton::AssignLamCfEqnName(int cf_case)
{
    string eqn_name;
    switch (cf_case)
    {
    case vsp::CF_LAM_BLASIUS:
        eqn_name = "Blasius";
        break;

    case vsp::CF_LAM_BLASIUS_W_HEAT:
        eqn_name = "Blasius w Heat Transfer";
        break;

    default:
        eqn_name = "ERROR";
    }
    return eqn_name;
}

double ParasiteDragMgrSingleton::CalcFFWing(double toc, int ff_case,
    double perc_lam = 0, double sweep25 = 0, double sweep50 = 0)
{
    // Values recreated using Plot Digitizer and fitted to a 3rd power polynomial
    double Interval[] = { 0.25, 0.6, 0.8, 0.9 };
    size_t nint = 4;
    double ff;

    switch (ff_case)
    {
    case vsp::FF_W_MANUAL:
        ff = 1;
        break;

    case vsp::FF_W_EDET_CONV:
        ff = 1.0 + toc*(2.94206 + toc*(7.16974 + toc*(48.8876 +
            toc*(-1403.02 + toc*(8598.76 + toc*(-15834.3))))));
        break;

    case vsp::FF_W_EDET_ADV:
        ff = 1.0 + 4.275 * toc;
        break;

    case vsp::FF_W_HOERNER:
        ff = 1.0 + 2.0 * toc + 60.0 * pow(toc, 4.0);
        break;

    case vsp::FF_W_COVERT:
        ff = 1.0 + 1.8 * toc + (50.0 * pow(toc, 4.0));
        break;

    case vsp::FF_W_SHEVELL:
        double Z;
        Z = ((2.0 - pow(m_Mach(), 2.0)) * cos(sweep25)) / (sqrt(1.0 -
            (pow(m_Mach(), 2.0) * pow(cos(sweep25), 2))));
        ff = 1.0 + (Z * toc) + (100.0 * pow(toc, 4.0));
        break;

    case vsp::FF_W_KROO:
        double part1A, part1B, part2A, part2B;
        part1A = (2.2 * pow(cos(sweep25), 2.0) * toc);
        part1B = (sqrt(1.0 - (pow(m_Mach(), 2.0) * pow(cos(sweep25), 2.0))));
        part2A = (4.84 * pow(cos(sweep25), 2.0)* (1.0 + 5.0 * pow(cos(sweep25), 2.0)) * pow(toc, 2.0));
        part2B = (2.0 * (1.0 - (pow(m_Mach(), 2.0) * pow(cos(sweep25), 2.0))));
        ff = 1.0 + (part1A / part1B) + (part2A / part2B);
        break;

    case vsp::FF_W_TORENBEEK:
        ff = 1.0 + 2.7*toc + 100.0 * pow(toc, 4.0);
        break;

    case vsp::FF_W_DATCOM:
        double L, Rls, x, RLS_Low, RLS_High;;

        // L value Decided based on xtrans/c
        if (perc_lam <= 0.30)
            L = 2.0;
        else
            L = 1.2;

        for (size_t i = 0; i < nint; ++i)
        {
            if (m_Mach() <= Interval[0])
            {
                Rls = -2.0292 * pow(cos(sweep25), 3.0) + 3.6345 * pow(cos(sweep25), 2.0) - 1.391 * cos(sweep25) + 0.8521;
            }
            else if (m_Mach() > Interval[3])
            {
                Rls = -1.8316 * pow(cos(sweep25), 3.0) + 3.3944 * pow(cos(sweep25), 2.0) - 1.3596 * cos(sweep25) + 1.1567;
            }
            else if (m_Mach() >= Interval[i])
            {
                x = (m_Mach() - Interval[i]) / (Interval[i + 1] - Interval[i]);
                if (i == 0)
                {
                    RLS_Low = -2.0292 * pow(cos(sweep25), 3.0) + 3.6345 * pow(cos(sweep25), 2.0) - 1.391 * cos(sweep25) + 0.8521;
                    RLS_High = -1.9735 * pow(cos(sweep25), 3.0) + 3.4504 * pow(cos(sweep25), 2.0) - 1.186 * cos(sweep25) + 0.858;
                }
                else if (i == 1)
                {
                    RLS_Low = -1.9735 * pow(cos(sweep25), 3.0) + 3.4504 * pow(cos(sweep25), 2.0) - 1.186 * cos(sweep25) + 0.858;
                    RLS_High = -1.6538 * pow(cos(sweep25), 3.0) + 2.865 * pow(cos(sweep25), 2.0) - 0.886 * cos(sweep25) + 0.934;
                }
                else if (i == 2)
                {
                    RLS_Low = -1.6538 * pow(cos(sweep25), 3.0) + 2.865 * pow(cos(sweep25), 2.0) - 0.886 * cos(sweep25) + 0.934;
                    RLS_High = -1.8316 * pow(cos(sweep25), 3.0) + 3.3944 * pow(cos(sweep25), 2.0) - 1.3596 * cos(sweep25) + 1.1567;
                }

                Rls = x * (RLS_High - RLS_Low) + RLS_Low;
            }
        }

        ff = (1.0 + (L * toc) + 100.0 * pow(toc, 4.0)) * Rls;
        break;

    case vsp::FF_W_SCHEMENSKY_6_SERIES_AF:
        ff = 1.0 + (1.44 * toc) + (2.0 * pow(toc, 2.0));
        break;

    case vsp::FF_W_SCHEMENSKY_4_SERIES_AF:
        ff = 1.0 + (1.68 * toc) + (3.0 * pow(toc, 2.0));
        break;

        //case vsp::FF_W_SCHEMENSKY_SUPERCRITICAL_AF :
        //    geo_ffName.push_back("Schemensky Supercritical AF");
        //    // Need Design Camber and Critical Mach #
        //    double K1, deltaM;

        //    deltaM = m_Mach() - Mcr;

        //    if ( deltaM <= -0.2 )
        //        K1 = 0.3;
        //    else if ( deltaM > 0.2 && deltaM < 0.0 )
        //        K1 = ( 6.7964 * pow( deltaM, 2 ) ) + ( 2.3605 * deltaM ) + 0.5059;
        //    else if ( deltaM >= 0.0 )
        //        K1 = 0.5;

        //    ff =  1 + ( K1* C1d ) + ( 1.44 * toc ) + ( 2 * pow( toc, 2 ) ) );
        //    break;

    case vsp::FF_W_JENKINSON_WING:
        double Fstar;

        Fstar = 1.0 + (3.3 * toc) - (0.008 * pow(toc, 2.0)) + (27.0 * pow(toc, 3.0));

        ff = ((Fstar - 1.0) * pow(cos(sweep50), 2.0)) + 1.0;
        break;

    case vsp::FF_W_JENKINSON_TAIL:

        Fstar = 1.0 + 3.52 * toc;

        ff = ((Fstar - 1.0) * pow(cos(sweep50), 2.0)) + 1.0;
        break;

    default:
        ff = 0;
    }
    return ff;
}

double ParasiteDragMgrSingleton::CalcFFBody(double longF, double FR, int ff_case, double ref_leng, double max_x_area)
{
    double ff;
    switch (ff_case)
    {
    case vsp::FF_B_MANUAL:
        ff = 1.0;
        break;

    case vsp::FF_B_SCHEMENSKY_FUSE:
        ff = 1.0 + (60.0 / pow(FR, 3.0)) + (0.0025 * FR);
        break;

    case vsp::FF_B_SCHEMENSKY_NACELLE:
        ff = 1.0 + 0.35 / FR;
        break;

    case vsp::FF_B_HOERNER_STREAMBODY:
        ff = 1.0 + (1.5 / pow(longF, 1.5)) +
            (7.0 / pow(longF, 3.0));
        break;

    case vsp::FF_B_TORENBEEK:
        ff = 1.0 + (2.2 / pow(longF, 1.5)) +
            (3.8 / pow(longF, 3.0));
        break;

    case vsp::FF_B_SHEVELL:
        ff = 1.0 + (2.8 / pow(longF, 1.5)) +
            (3.8 / pow(longF, 3.0));
        break;

    case vsp::FF_B_JENKINSON_FUSE:
        double Lambda;
        Lambda = ref_leng / (pow((4.0 / PI) * max_x_area, 0.5));

        ff = 1.0 + (2.2 / pow(Lambda, 1.5)) -
            (0.9 / pow(Lambda, 3.0));
        break;

    case vsp::FF_B_JENKINSON_WING_NACELLE:
        ff = 1.25;
        break;

    case vsp::FF_B_JENKINSON_AFT_FUSE_NACELLE:
        ff = 1.5;
        break;

    case vsp::FF_B_JOBE:
        ff = 1.02 + (1.5 / (pow(longF, 1.5))) + (7.0 / (0.6 * pow(longF, 3.0) * (1.0 - pow(m_Mach(), 3.0))));
        break;

    default:
        ff = 0.0;
        break;
    }
    return ff;
}

string ParasiteDragMgrSingleton::AssignFFWingEqnName(int ff_case)
{
    string ff_name;
    switch (ff_case)
    {
    case vsp::FF_W_MANUAL:
        ff_name = "Manual";
        break;

    case vsp::FF_W_EDET_CONV:
        ff_name = "EDET Conventional";
        break;

    case vsp::FF_W_EDET_ADV:
        ff_name = "EDET Advanced";
        break;

    case vsp::FF_W_HOERNER:
        ff_name = "Hoerner";
        break;

    case vsp::FF_W_COVERT:
        ff_name = "Covert";
        break;

    case vsp::FF_W_SHEVELL:
        ff_name = "Shevell";
        break;

    case vsp::FF_W_KROO:
        ff_name = "Kroo";
        break;

    case vsp::FF_W_TORENBEEK:
        ff_name = "Torenbeek";
        break;

    case vsp::FF_W_DATCOM:
        ff_name = "DATCOM";
        break;

    case vsp::FF_W_SCHEMENSKY_6_SERIES_AF:
        ff_name = "Schemensky 6 Series AF";
        break;

    case vsp::FF_W_SCHEMENSKY_4_SERIES_AF:
        ff_name = "Schemensky 4 Series AF";
        break;

        //case vsp::FF_W_SCHEMENSKY_SUPERCRITICAL_AF :
        //    ff_name = "Schemensky Supercritical AF");
        //    break;

    case vsp::FF_W_JENKINSON_WING:
        ff_name = "Jenkinson Wing";
        break;

    case vsp::FF_W_JENKINSON_TAIL:
        ff_name = "Jenkinson Tail";
        break;

    default:
        ff_name = "ERROR";
    }
    return ff_name;
}

string ParasiteDragMgrSingleton::AssignFFBodyEqnName(int ff_case)
{
    string ff_name;
    switch (ff_case)
    {
    case vsp::FF_B_MANUAL:
        ff_name = "Manual";
        break;

    case vsp::FF_B_SCHEMENSKY_FUSE:
        ff_name = "Schemensky Fuselage";
        break;

    case vsp::FF_B_SCHEMENSKY_NACELLE:
        ff_name = "Schemensky Nacelle";
        break;

    case vsp::FF_B_HOERNER_STREAMBODY:
        ff_name = "Hoerner Streamlined Body";
        break;

    case vsp::FF_B_TORENBEEK:
        ff_name = "Torenbeek";
        break;

    case vsp::FF_B_SHEVELL:
        ff_name = "Shevell";
        break;

    case vsp::FF_B_JENKINSON_FUSE:
        ff_name = "Jenkinson Fuselage";
        break;

    case vsp::FF_B_JENKINSON_WING_NACELLE:
        ff_name = "Jenkinson Wing Nacelle";
        break;

    case vsp::FF_B_JENKINSON_AFT_FUSE_NACELLE:
        ff_name = "Jenkinson Aft Fuse Nacelle";
        break;

    case vsp::FF_B_JOBE:
        ff_name = "Jobe";
        break;

    default:
        ff_name = "ERROR";
        break;
    }
    return ff_name;
}

void ParasiteDragMgrSingleton::SetActiveGeomVec()
{
    Vehicle* veh = VehicleMgr.GetVehicle();
    vector <string> geomVec = veh->GetGeomSet(m_SetChoice());

    m_PDGeomIDVec.clear();
    for (int i = 0; i < geomVec.size(); i++)
    {
        Geom* geom = veh->FindGeom(geomVec[i]);
        if (geom->GetType().m_Type != HINGE_GEOM_TYPE && geom->GetType().m_Type != BLANK_GEOM_TYPE)
        {
            if (geom->GetSurfPtr(0)->GetSurfType() != vsp::DISK_SURF)
            {
                m_PDGeomIDVec.push_back(geomVec[i]);
            }
        }
    }
}

void ParasiteDragMgrSingleton::SetFreestreamParms()
{
    m_Temp.Set(m_Atmos.GetTemp());
    m_Pres.Set(m_Atmos.GetPres());
    m_Rho.Set(m_Atmos.GetDensity());
    m_DynaVisc.Set(m_Atmos.GetDynaVisc());
}

void ParasiteDragMgrSingleton::SetExcresLabel(const string & newLabel)
{
    if (m_CurrentExcresIndex != -1)
    {
        m_ExcresRowVec[m_CurrentExcresIndex].Label = newLabel;
    }
}

double ParasiteDragMgrSingleton::GetLrefSigFig()
{
    double lrefmag;
    if (!geo_lref.empty())
    {
        vector<double>::const_iterator it = max_element(geo_lref.begin(), geo_lref.end());
        lrefmag = mag(*it);
    }
    else
    {
        lrefmag = 1;
    }

    if (lrefmag > 1)
    {
        return 1;
    }
    else if (lrefmag == 1)
    {
        return 2;
    }
    else
    {
        return 3;
    }
}

double ParasiteDragMgrSingleton::GetGeometryCd()
{
    double sum = 0;
    for (int i = 0; i < geo_Cd.size(); i++)
    {
        if (geo_Cd[i] > 0.0)
        {
            sum += geo_Cd[i];
        }
    }
    return sum;
}

double ParasiteDragMgrSingleton::GetSubTotalCD()
{
    return GetGeometryCd() + GetSubTotalExcresCd();
}

double ParasiteDragMgrSingleton::GetTotalCD()
{
    for (size_t i = 0; i < m_ExcresRowVec.size(); ++i)
    {
        if (m_ExcresRowVec[i].Type == vsp::EXCRESCENCE_MARGIN)
        {
            return GetGeometryCd() + GetTotalExcresCD();
        }
    }
    return GetSubTotalCD();
}

vector<string> ParasiteDragMgrSingleton::GetExcresIDs()
{
    vector < string > vec;
    for (size_t i = 0; i < m_ExcresRowVec.size(); ++i)
    {
        vec.push_back(m_ExcresRowVec[i].Label);
    }
    return vec;
}

string ParasiteDragMgrSingleton::GetCurrentExcresLabel()
{
    if (m_CurrentExcresIndex != -1)
    {
        return m_ExcresRowVec[m_CurrentExcresIndex].Label;
    }
    return "";
}

string ParasiteDragMgrSingleton::GetCurrentExcresTypeString()
{
    if (m_CurrentExcresIndex != -1)
    {
        return m_ExcresRowVec[m_CurrentExcresIndex].TypeString;
    }
    return "";
}

double ParasiteDragMgrSingleton::GetCurrentExcresValue()
{
    if (m_CurrentExcresIndex != -1)
    {
        return m_ExcresRowVec[m_CurrentExcresIndex].Input;
    }
    return 0;
}

int ParasiteDragMgrSingleton::GetCurrentExcresType()
{
    if (m_CurrentExcresIndex != -1)
    {
        return m_ExcresRowVec[m_CurrentExcresIndex].Type;
    }
    return 0;
}

void ParasiteDragMgrSingleton::AddExcrescence()
{
    ExcrescenceTableRow tempStruct;
    ostringstream strs;
    char str[256];

    if (m_ExcresRowVec.size() > 0)
    {
        for (size_t i = 0; i < m_ExcresRowVec.size(); ++i)
        {
            if (m_ExcresRowVec[i].Type == vsp::EXCRESCENCE_MARGIN && m_ExcresType() == vsp::EXCRESCENCE_MARGIN)
            {
                return;
            }
        }
    }

    if (m_ExcresName.empty())
    {
        sprintf(str, "EXCRES_%i", m_ExcresRowVec.size());
        tempStruct.Label = str;
    }
    else
    {
        tempStruct.Label = m_ExcresName;
    }

    m_ExcresName.clear();

    tempStruct.Input = m_ExcresValue.Get();

    if (m_ExcresType() == vsp::EXCRESCENCE_COUNT)
    {
        tempStruct.Amount = m_ExcresValue() / 10000.0;
        tempStruct.TypeString = "Count (10000*CD)";
    }
    else if (m_ExcresType() == vsp::EXCRESCENCE_CD)
    {
        tempStruct.Amount = m_ExcresValue();
        tempStruct.TypeString = "CD";
    }
    else if (m_ExcresType() == vsp::EXCRESCENCE_PERCENT_GEOM)
    {
        tempStruct.Amount = 0.0; // Calculated in UpdateExcres()
        tempStruct.TypeString = "% of Cd_Geom";
    }
    else if (m_ExcresType() == vsp::EXCRESCENCE_MARGIN)
    {
        tempStruct.Amount = 0.0; // Calculated in UpdateExcres()
        tempStruct.TypeString = "Margin";
    }
    else if (m_ExcresType() == vsp::EXCRESCENCE_DRAGAREA)
    {
        tempStruct.Amount = 0.0; // Calculated in UpdateExcres()
        tempStruct.TypeString = "Drag Area (D/q)";
    }

    tempStruct.Type = m_ExcresType();

    tempStruct.f = tempStruct.Amount * m_Sref();

    tempStruct.PercTotalCd = 0.0; // Initializing this to 0.0

    m_ExcresRowVec.push_back(tempStruct);

    m_CurrentExcresIndex = m_ExcresRowVec.size() - 1;
}

void ParasiteDragMgrSingleton::AddExcrescence(const std::string & excresName, int excresType, double excresVal)
{
    m_ExcresValue.Set(excresVal);
    m_ExcresType.Set(excresType);
    m_ExcresName = excresName;

    AddExcrescence();
}

void ParasiteDragMgrSingleton::DeleteExcrescence()
{
    if (m_CurrentExcresIndex != -1)
    {
        m_ExcresRowVec.erase(m_ExcresRowVec.begin() + m_CurrentExcresIndex);
    }

    if (m_ExcresRowVec.size() > 0)
    {
        m_CurrentExcresIndex = 0;
        UpdateCurrentExcresVal();
    }
    else
    {
        m_CurrentExcresIndex = -1;
    }
}

void ParasiteDragMgrSingleton::DeleteExcrescence(int index)
{
    m_CurrentExcresIndex = index;
    DeleteExcrescence();
}

double ParasiteDragMgrSingleton::CalcPercentageGeomCd(double val)
{
    if (!m_DegenGeomVec.empty())
    {
        if (GetGeometryCd() > 0.0)
        {
            return val / 100.0 * GetGeometryCd();
        }
        else
        {
            return 0;
        }
    }

    return 0;
}

double ParasiteDragMgrSingleton::CalcPercentageTotalCD(double val)
{
    if (!m_DegenGeomVec.empty())
    {
        if (GetSubTotalCD() > 0.0)
        {
            return GetSubTotalCD()/((100.0-val)/100.0) - GetSubTotalCD();
        }
        else
        {
            return 0;
        }
    }

    return 0;
}

double ParasiteDragMgrSingleton::CalcDragAreaCd(double val)
{
    if (!m_DegenGeomVec.empty())
    {
        if (GetGeometryCd() > 0)
        {
            return val / m_Sref.Get();
        }
        else
        {
            return 0;
        }
    }

    return 0;
}

double ParasiteDragMgrSingleton::GetSubTotalExcresCd()
{
    double sum = 0;

    for (size_t i = 0; i < m_ExcresRowVec.size(); ++i)
    {
        if (m_ExcresRowVec[i].Type != vsp::EXCRESCENCE_MARGIN)
        {
            sum += m_ExcresRowVec[i].Amount;
        }
    }

    return sum;
}

double ParasiteDragMgrSingleton::GetTotalExcresCD()
{
    double sum = 0;

    for (size_t i = 0; i < m_ExcresRowVec.size(); ++i)
    {
        sum += m_ExcresRowVec[i].Amount;
    }

    return sum;
}

void ParasiteDragMgrSingleton::ConsolidateExcres()
{
    excres_Label.clear();
    excres_Type.clear();
    excres_Input.clear();
    excres_Amount.clear();
    excres_PercTotalCd.clear();
    ExcrescenceTableRow excresRowStruct;
    for (size_t i = 0; i < m_ExcresRowVec.size(); ++i)
    {
        excres_Label.push_back(m_ExcresRowVec[i].Label.c_str());
        excres_Type.push_back(m_ExcresRowVec[i].TypeString.c_str());
        excres_Input.push_back(m_ExcresRowVec[i].Input);
        excres_Amount.push_back(m_ExcresRowVec[i].Amount);
        excres_PercTotalCd.push_back(m_ExcresRowVec[i].PercTotalCd);
    }
}

void ParasiteDragMgrSingleton::Update()
{
    // Update Filename
    ParasiteDragMgr.m_FileName = VehicleMgr.GetVehicle()->getExportFileName(vsp::DRAG_BUILD_CSV_TYPE);

    UpdateRefWing();

    UpdateTempLimits();
    UpdateAtmos();

    UpdateParmActivity();

    SetFreestreamParms();

    UpdateExcres();
}

void ParasiteDragMgrSingleton::UpdateWettedAreaTotals()
{
    Vehicle* veh = VehicleMgr.GetVehicle();
    if (!m_DegenGeomVec.empty())
    {
        // Subsurfaces Addition First
        for (size_t i = 0; i < m_RowSize; ++i)
        {
            for (size_t j = 0; j < m_RowSize; ++j)
            {
                if (i != j) // If not the same geom
                {
                    if (geo_subsurfID[i].compare("") == 0 && geo_subsurfID[j].compare("") != 0)
                    {
                        if (!veh->FindGeom(geo_geomID[i])->m_ExpandedListFlag() &&
                            geo_geomID[i].compare(geo_geomID[j]) == 0 &&
                            veh->FindGeom(geo_geomID[j])->GetSubSurf(geo_subsurfID[j])->m_IncludeFlag() &&
                            geo_surfNum[i] == 0)
                        {
                            geo_swet[i] += geo_swet[j];
                        }
                        else if (!veh->FindGeom(geo_geomID[i])->m_ExpandedListFlag() &&
                            geo_geomID[i].compare(veh->FindGeom(geo_geomID[j])->GetAncestorID(geo_groupedAncestorGen[j])) == 0 &&
                            veh->FindGeom(geo_geomID[j])->GetSubSurf(geo_subsurfID[j])->m_IncludeFlag() &&
                            geo_surfNum[i] == 0)
                        {
                            geo_swet[i] += geo_swet[j];
                        }
                    }
                }
            }
        }

        // Add Geom Surf Wetted Areas
        for (size_t i = 0; i < m_RowSize; ++i)
        {
            for (size_t j = 0; j < m_RowSize; ++j)
            {
                if (i != j) // If not the same geom
                {
                    if (geo_subsurfID[i].compare("") == 0 && geo_subsurfID[j].compare("") == 0)
                    {
                        // IF
                        // ==========
                        // Same Geom AND Geom[i] is main surface
                        // OR
                        // Not the same Geom AND Ancestor of Geom[j] is Geom[i] AND Geom[i] is 0th surface AND Geom[j] not an expanded list
                        // OR
                        // is custom geom (TODO: this could use some work)
                        // ==========
                        // AND
                        // ==========
                        // Shape types are the same AND Geom[i] is NOT an expanded list
                        if (((geo_geomID[i].compare(geo_geomID[j]) == 0 && geo_surfNum[i] == 0) ||
                            (geo_geomID[i].compare(geo_geomID[j]) != 0 &&
                                geo_geomID[i].compare(veh->FindGeom(geo_geomID[j])->GetAncestorID(geo_groupedAncestorGen[j])) == 0 &&
                                geo_surfNum[i] == 0 && !veh->FindGeom(geo_geomID[j])->m_ExpandedListFlag()) ||
                                (geo_label[i].substr(0, 3).compare("[W]") == 0 || geo_label[i].substr(0, 3).compare("[B]") == 0)) &&
                            (geo_shapeType[i] == geo_shapeType[j] && !geo_expandedList[i]))
                        {
                            geo_swet[i] += geo_swet[j];
                        }
                    }
                }
            }
        }
    }
}

void ParasiteDragMgrSingleton::UpdateRefWing()
{
    // Update Reference Area Section 
    if (m_RefFlag() == MANUAL_REF)
    { // Allow Manual Input
        m_Sref.Activate();
    }
    else
    { // Pull from existing geometry
        Geom* refgeom = VehicleMgr.GetVehicle()->FindGeom(m_RefGeomID);

        if (refgeom)
        {
            if (refgeom->GetType().m_Type == MS_WING_GEOM_TYPE)
            {
                WingGeom* refwing = (WingGeom*)refgeom;
                m_Sref.Set(refwing->m_TotalArea());

                m_Sref.Deactivate();
            }
        }
        else
        {
            m_RefGeomID = string();
        }
    }
}

void ParasiteDragMgrSingleton::UpdateAtmos()
{
    double LqRe;
    double vinf, temp, pres, rho, dynavisc;
    vinf = m_Vinf();
    temp = m_Temp();
    pres = m_Pres();
    rho = m_Rho();
    dynavisc = m_DynaVisc();

    // Determine Which Atmos Variant will Calculate Atmospheric Properties
    if (m_FreestreamType() == vsp::ATMOS_TYPE_US_STANDARD_1976)
    {
        m_Atmos.USStandardAtmosphere1976(m_Hinf(), m_DeltaT(), m_AltLengthUnit(), m_TempUnit.Get(), m_PresUnit());
        m_Atmos.UpdateMach(vinf, m_SpecificHeatRatio(), m_TempUnit(), m_VinfUnitType());
    }
    else if (m_FreestreamType() == vsp::ATMOS_TYPE_HERRINGTON_1966)
    {
        m_Atmos.USAF1966(m_Hinf(), m_DeltaT(), m_AltLengthUnit(), m_TempUnit.Get(), m_PresUnit());
        m_Atmos.UpdateMach(vinf, m_SpecificHeatRatio(), m_TempUnit(), m_VinfUnitType());
    }
    else if (m_FreestreamType() == vsp::ATMOS_TYPE_MANUAL_P_R)
    {
        m_Atmos.SetManualQualities(vinf, temp, pres, rho, dynavisc, m_SpecificHeatRatio(),
            m_AltLengthUnit(), m_VinfUnitType(), m_TempUnit(), m_FreestreamType());
    }
    else if (m_FreestreamType() == vsp::ATMOS_TYPE_MANUAL_P_T)
    {
        m_Atmos.SetManualQualities(vinf, temp, pres, rho, dynavisc, m_SpecificHeatRatio(),
            m_AltLengthUnit(), m_VinfUnitType(), m_TempUnit(), m_FreestreamType());
    }
    else if (m_FreestreamType() == vsp::ATMOS_TYPE_MANUAL_R_T)
    {
        m_Atmos.SetManualQualities(vinf, temp, pres, rho, dynavisc, m_SpecificHeatRatio(),
            m_AltLengthUnit(), m_VinfUnitType(), m_TempUnit(), m_FreestreamType());
    }
    else if (m_FreestreamType() == vsp::ATMOS_TYPE_MANUAL_RE_L)
    {
        vinf = m_Atmos.GetMach() * m_Atmos.GetSoundSpeed();
        UpdateVinf(m_VinfUnitType());
    }

    if (m_FreestreamType() != vsp::ATMOS_TYPE_MANUAL_RE_L)
    {
        m_Hinf.Set(m_Atmos.GetAlt());
        m_DeltaT.Set(m_Atmos.GetDeltaT());
        m_Temp.Set(m_Atmos.GetTemp());
        m_Pres.Set(m_Atmos.GetPres());
        m_Rho.Set(m_Atmos.GetDensity());
        m_Mach.Set(m_Atmos.GetMach());

        if (m_AltLengthUnit() == vsp::PD_UNITS_IMPERIAL)
        {
            vinf = ConvertVelocity(vinf, m_VinfUnitType.Get(), vsp::V_UNIT_FT_S);
        }
        else if (m_AltLengthUnit() == vsp::PD_UNITS_METRIC)
        {
            vinf = ConvertVelocity(vinf, m_VinfUnitType.Get(), vsp::V_UNIT_M_S);
        }

        m_KineVisc = m_Atmos.GetDynaVisc() / m_Rho();

        LqRe = m_KineVisc() / vinf;

        if (m_AltLengthUnit() == vsp::PD_UNITS_IMPERIAL)
        {
            vinf = ConvertLength(LqRe, vsp::LEN_FT, m_LengthUnit());
        }
        else if (m_AltLengthUnit() == vsp::PD_UNITS_METRIC)
        {
            vinf = ConvertLength(LqRe, vsp::LEN_M, m_LengthUnit());
        }

        m_ReqL.Set(1.0 / LqRe);
    }
}

void ParasiteDragMgrSingleton::UpdateVinf(int newunit)
{
    double new_vinf;
    if (m_VinfUnitType() == vsp::V_UNIT_KEAS)
    {
        m_Vinf *= sqrt(1.0 / m_Atmos.GetDensityRatio());
    }
    new_vinf = ConvertVelocity(m_Vinf(), m_VinfUnitType(), newunit);
    if (newunit == vsp::V_UNIT_KEAS)
    {
        new_vinf /= sqrt(1.0 / m_Atmos.GetDensityRatio());
    }
    new_vinf = ConvertVelocity(m_Vinf(), m_VinfUnitType(), newunit);
    m_Vinf.Set(new_vinf);
    m_VinfUnitType.Set(newunit);
}

void ParasiteDragMgrSingleton::UpdateAlt(int newunit)
{
    double new_alt;
    if (newunit == vsp::PD_UNITS_IMPERIAL && m_AltLengthUnit() == vsp::PD_UNITS_METRIC)
    {
        new_alt = ConvertLength(m_Hinf(), vsp::LEN_M, vsp::LEN_FT);
    }
    else if (newunit == vsp::PD_UNITS_METRIC && m_AltLengthUnit() == vsp::PD_UNITS_IMPERIAL)
    {
        new_alt = ConvertLength(m_Hinf(), vsp::LEN_FT, vsp::LEN_M);
    }

    m_Hinf.Set(new_alt);
    m_AltLengthUnit.Set(newunit);
}

void ParasiteDragMgrSingleton::UpdateAltLimits()
{
    switch (m_AltLengthUnit())
    {
    case vsp::PD_UNITS_IMPERIAL:
        m_Hinf.SetUpperLimit(278385.83);
        break;

    case vsp::PD_UNITS_METRIC:
        m_Hinf.SetUpperLimit(84852.0);
        break;

    default:
        break;
    }
}

void ParasiteDragMgrSingleton::UpdateTemp(int newunit)
{
    double new_temp = ConvertTemperature(m_Temp(), m_TempUnit(), newunit);
    m_Temp.Set(new_temp);
    m_TempUnit.Set(newunit);
}

void ParasiteDragMgrSingleton::UpdateTempLimits()
{
    switch (m_TempUnit())
    {
    case vsp::TEMP_UNIT_C:
        m_Temp.SetLowerLimit(-273.15);
        break;

    case vsp::TEMP_UNIT_F:
        m_Temp.SetLowerLimit(-459.666);
        break;

    case vsp::TEMP_UNIT_K:
        m_Temp.SetLowerLimit(0.0);
        break;

    case vsp::TEMP_UNIT_R:
        m_Temp.SetLowerLimit(0.0);
        break;
    }
}

void ParasiteDragMgrSingleton::UpdatePres(int newunit)
{
    double new_pres = ConvertPressure(m_Pres(), m_PresUnit(), newunit);
    m_Pres.Set(new_pres);
    m_PresUnit.Set(newunit);
}

void ParasiteDragMgrSingleton::UpdatePercentageCD()
{
    double totalCd0 = GetTotalCD();
    double ftotal = 0;
    double percTotal = 0;

    for (int i = 0; i < geo_Cd.size(); i++)
    {
        if (!m_DegenGeomVec.empty())
        {
            if (!isnan(geo_f[i]))
            {
                geo_percTotalCd.push_back(geo_Cd[i] / totalCd0);
                percTotal += geo_Cd[i] / totalCd0;
                ftotal += geo_f[i];
            }
            else
            {
                geo_percTotalCd.push_back(0.0);
            }
        }
        else
        {
            geo_percTotalCd.push_back(0);
            percTotal += 0;
        }
    }

    m_GeomfTotal = ftotal;
    m_GeomPercTotal = percTotal;

    ftotal = 0;
    percTotal = 0;
    for (size_t i = 0; i < m_ExcresRowVec.size(); ++i)
    {
        if (!m_DegenGeomVec.empty())
        {
            m_ExcresRowVec[i].PercTotalCd = m_ExcresRowVec[i].Amount / totalCd0;
            percTotal += m_ExcresRowVec[i].Amount / totalCd0;
            ftotal += m_ExcresRowVec[i].f;
        }
        else
        {
            m_ExcresRowVec[i].PercTotalCd = 0.0;
        }
    }

    m_ExcresfTotal = ftotal;
    m_ExcresPercTotal = percTotal;
}


void ParasiteDragMgrSingleton::UpdateParmActivity()
{
    // Activate/Deactivate Appropriate Flow Condition Parameters
    DeactivateParms();

    if (m_FreestreamType() == vsp::ATMOS_TYPE_US_STANDARD_1976 || m_FreestreamType() == vsp::ATMOS_TYPE_HERRINGTON_1966)
    {
        m_Vinf.Activate();
        m_Hinf.Activate();
    }
    else if (m_FreestreamType() == vsp::ATMOS_TYPE_MANUAL_P_R)
    {
        m_Vinf.Activate();
        m_Pres.Activate();
        m_Rho.Activate();
        m_SpecificHeatRatio.Activate();
    }
    else if (m_FreestreamType() == vsp::ATMOS_TYPE_MANUAL_P_T)
    {
        m_Vinf.Activate();
        m_Temp.Activate();
        m_Pres.Activate();
        m_SpecificHeatRatio.Activate();
    }
    else if (m_FreestreamType() == vsp::ATMOS_TYPE_MANUAL_R_T)
    {
        m_Vinf.Activate();
        m_Temp.Activate();
        m_Rho.Activate();
        m_SpecificHeatRatio.Activate();
    }
    else if (m_FreestreamType() == vsp::ATMOS_TYPE_MANUAL_RE_L)
    {
        m_ReqL.Activate();
        m_Mach.Activate();
        m_SpecificHeatRatio.Activate();
    }
}

void ParasiteDragMgrSingleton::UpdateExportLabels()
{
    string deg(1, 176);
    string squared(1, 178);
    string cubed(1, 179);

    // Flow Qualities
    switch (m_AltLengthUnit.Get())
    {
    case vsp::PD_UNITS_IMPERIAL:
        m_RhoLabel = "Density (slug/ft^3)";
        m_AltLabel = "Altitude (ft)";
        break;

    case vsp::PD_UNITS_METRIC:
        m_RhoLabel = "Density (kg/m^3)";
        m_AltLabel = "Altitude (m)";
        break;
    }

    // Length
    switch (m_LengthUnit.Get())
    {
    case vsp::LEN_MM:
        m_LrefLabel = "L_ref (mm)";
        m_SrefLabel = "S_ref (mm^2)";
        m_fLabel = "f (mm^2)";
        m_SwetLabel = "S_wet (mm^2)";
        break;

    case vsp::LEN_CM:
        m_LrefLabel = "L_ref (cm)";
        m_SrefLabel = "S_ref (cm^2)";
        m_fLabel = "f (cm^2)";
        m_SwetLabel = "S_wet (cm^2)";
        break;

    case vsp::LEN_M:
        m_LrefLabel = "L_ref (m)";
        m_SrefLabel = "S_ref (m^2)";
        m_fLabel = "f (m^2)";
        m_SwetLabel = "S_wet (m^2)";
        break;

    case vsp::LEN_IN:
        m_LrefLabel = "L_ref (in)";
        m_SrefLabel = "S_ref (in^2)";
        m_fLabel = "f (in^2)";
        m_SwetLabel = "S_wet (in^2)";
        break;

    case vsp::LEN_FT:
        m_LrefLabel = "L_ref (ft)";
        m_SrefLabel = "S_ref (ft^2)";
        m_fLabel = "f (ft^2)";
        m_SwetLabel = "S_wet (ft^2)";
        break;

    case vsp::LEN_YD:
        m_LrefLabel = "L_ref (yd)";
        m_SrefLabel = "S_ref (yd^2)";
        m_fLabel = "f (yd^2)";
        m_SwetLabel = "S_wet (yd^2)";
        break;

    case vsp::LEN_UNITLESS:
        m_LrefLabel = "L_ref (LU)";
        m_SrefLabel = "S_ref (LU^2)";
        m_fLabel = "f (LU^2)";
        m_SwetLabel = "S_wet (LU^2)";
        break;
    }

    // Vinf
    switch (m_VinfUnitType.Get())
    {
    case vsp::V_UNIT_FT_S:
        m_VinfLabel = "Vinf (ft/s)";
        break;

    case vsp::V_UNIT_M_S:
        m_VinfLabel = "Vinf (m/s)";
        break;

    case vsp::V_UNIT_KEAS:
        m_VinfLabel = "Vinf (KEAS)";
        break;

    case vsp::V_UNIT_KTAS:
        m_VinfLabel = "Vinf (KTAS)";
        break;

    case vsp::V_UNIT_MPH:
        m_VinfLabel = "Vinf (mph)";
        break;

    case vsp::V_UNIT_KM_HR:
        m_VinfLabel = "Vinf (km/hr)";
        break;
    }

    // Temperature
    switch (m_TempUnit.Get())
    {
    case vsp::TEMP_UNIT_C:
        m_TempLabel = "Temp (" + deg + "C)";
        break;

    case vsp::TEMP_UNIT_F:
        m_TempLabel = "Temp (" + deg + "F)";
        break;

    case vsp::TEMP_UNIT_K:
        m_TempLabel = "Temp (K)";
        break;

    case vsp::TEMP_UNIT_R:
        m_TempLabel = "Temp (" + deg + "R)";
        break;
    }

    // Pressure
    switch (m_PresUnit())
    {
    case vsp::PRES_UNIT_PSF:
        m_PresLabel = "Pressure (lbf/ft^2)";
        break;

    case vsp::PRES_UNIT_PSI:
        m_PresLabel = "Pressure (lbf/in^2)";
        break;

    case vsp::PRES_UNIT_PA:
        m_PresLabel = "Pressure (Pa)";
        break;

    case vsp::PRES_UNIT_KPA:
        m_PresLabel = "Pressure (kPa)";
        break;

    case vsp::PRES_UNIT_INCHHG:
        m_PresLabel = "Pressure (\"Hg)";
        break;

    case vsp::PRES_UNIT_MMHG:
        m_PresLabel = "Pressure (mmHg)";
        break;

    case vsp::PRES_UNIT_MMH20:
        m_PresLabel = "Pressure (mmH20)";
        break;

    case vsp::PRES_UNIT_MB:
        m_PresLabel = "Pressure (mB)";
        break;

    case vsp::PRES_UNIT_ATM:
        m_PresLabel = "Pressure (atm)";
        break;
    }
}

void ParasiteDragMgrSingleton::UpdateExcres()
{
    // Updates Current Excres Value & Updates Excres Values that are a % of Cd_Geom
    for (size_t i = 0; i < m_ExcresRowVec.size(); ++i)
    {
        if (i == m_CurrentExcresIndex)
        {
            m_ExcresRowVec[i].Input = m_ExcresValue();

            if (m_ExcresRowVec[i].Type == vsp::EXCRESCENCE_CD)
            {
                m_ExcresRowVec[i].Amount = m_ExcresValue();
            }
            else if (m_ExcresRowVec[i].Type == vsp::EXCRESCENCE_COUNT)
            {
                m_ExcresRowVec[i].Amount = m_ExcresValue() / 10000;
            }
            else if (m_ExcresRowVec[i].Type == vsp::EXCRESCENCE_PERCENT_GEOM)
            {
                m_ExcresRowVec[i].Amount = CalcPercentageGeomCd(m_ExcresValue());
            }
            else if (m_ExcresRowVec[i].Type == vsp::EXCRESCENCE_MARGIN)
            {
                m_ExcresRowVec[i].Amount = CalcPercentageTotalCD(m_ExcresValue());
            }
            else if (m_ExcresRowVec[i].Type == vsp::EXCRESCENCE_DRAGAREA)
            {
                m_ExcresRowVec[i].Amount = CalcDragAreaCd(m_ExcresValue());
            }
        }
        else
        {
            if (m_ExcresRowVec[i].Type == vsp::EXCRESCENCE_PERCENT_GEOM)
            {
                m_ExcresRowVec[i].Amount = CalcPercentageGeomCd(m_ExcresRowVec[i].Input);
            }
            else if (m_ExcresRowVec[i].Type == vsp::EXCRESCENCE_MARGIN)
            {
                m_ExcresRowVec[i].Amount = CalcPercentageTotalCD(m_ExcresRowVec[i].Input);
            }
            else if (m_ExcresRowVec[i].Type == vsp::EXCRESCENCE_DRAGAREA)
            {
                m_ExcresRowVec[i].Amount = CalcDragAreaCd(m_ExcresRowVec[i].Input);
            }
        }
    }

    // Calculates Individual f
    for (size_t i = 0; i < m_ExcresRowVec.size(); ++i)
    {
        if (GetSubTotalCD() > 0.0)
        {
            m_ExcresRowVec[i].f = m_ExcresRowVec[i].Amount * m_Sref.Get();
        }
    }

    ConsolidateExcres();
}

void ParasiteDragMgrSingleton::UpdateCurrentExcresVal()
{
    m_ExcresType.Set(m_ExcresRowVec[m_CurrentExcresIndex].Type);
    if (m_ExcresType() == vsp::EXCRESCENCE_CD)
    {
        m_ExcresValue.SetLowerUpperLimits(0.0, 0.2);
    }
    else if (m_ExcresType() == vsp::EXCRESCENCE_COUNT)
    {
        m_ExcresValue.SetLowerUpperLimits(0.0, 2000.0);
    }
    else if (m_ExcresType() == vsp::EXCRESCENCE_PERCENT_GEOM)
    {
        m_ExcresValue.SetLowerUpperLimits(0.0, 100.0);
    }
    else if (m_ExcresType() == vsp::EXCRESCENCE_MARGIN)
    {
        m_ExcresValue.SetLowerUpperLimits(0.0, 100.0);
    }
    else if (m_ExcresType() == vsp::EXCRESCENCE_DRAGAREA)
    {
        m_ExcresValue.SetLowerUpperLimits(0.0, 10.0);
    }
    m_ExcresValue.Set(m_ExcresRowVec[m_CurrentExcresIndex].Input);
}

string ParasiteDragMgrSingleton::ExportToCSV()
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    // Create Results
    Results* res = ResultsMgr.CreateResults("Parasite_Drag");

    // Add Results to ResultsMgr
    UpdateExcres();
    UpdateExportLabels();

    res->Add(NameValData("Alt_Label", m_AltLabel));
    res->Add(NameValData("Vinf_Label", m_VinfLabel));
    res->Add(NameValData("Sref_Label", m_SrefLabel));
    res->Add(NameValData("Temp_Label", m_TempLabel));
    res->Add(NameValData("Pres_Label", m_PresLabel));
    res->Add(NameValData("Rho_Label", m_RhoLabel));
    res->Add(NameValData("LamCfEqnName", m_LamCfEqnName));
    res->Add(NameValData("TurbCfEqnName", m_TurbCfEqnName));
    res->Add(NameValData("Swet_Label", m_SwetLabel));
    res->Add(NameValData("Lref_Label", m_LrefLabel));
    res->Add(NameValData("f_Label", m_fLabel));

    // Flow Condition
    res->Add(NameValData("FC_Mach", m_Mach()));
    res->Add(NameValData("FC_Alt", m_Hinf.Get()));
    res->Add(NameValData("FC_Vinf", m_Vinf.Get()));
    res->Add(NameValData("FC_Sref", m_Sref.Get()));
    res->Add(NameValData("FC_Temp", m_Temp.Get()));
    res->Add(NameValData("FC_Pres", m_Pres.Get()));
    res->Add(NameValData("FC_Rho", m_Rho.Get()));

    // Component Related
    res->Add(NameValData("Num_Comp", m_RowSize));
    res->Add(NameValData("Comp_ID", geo_geomID));
    res->Add(NameValData("Comp_Label", geo_label));
    res->Add(NameValData("Comp_Swet", geo_swet));
    res->Add(NameValData("Comp_Lref", geo_lref));
    res->Add(NameValData("Comp_Re", geo_Re));
    res->Add(NameValData("Comp_PercLam", geo_percLam));
    res->Add(NameValData("Comp_Cf", geo_cf));
    res->Add(NameValData("Comp_FineRat", geo_fineRat));
    res->Add(NameValData("Comp_FFEqn", geo_ffType));
    res->Add(NameValData("Comp_FFEqnName", geo_ffName));
    res->Add(NameValData("Comp_FFIn", geo_ffIn));
    res->Add(NameValData("Comp_FFOut", geo_ffOut));
    res->Add(NameValData("Comp_Roughness", geo_Roughness));
    res->Add(NameValData("Comp_TeTwRatio", geo_TeTwRatio));
    res->Add(NameValData("Comp_TawTwRatio", geo_TawTwRatio));
    res->Add(NameValData("Comp_Q", geo_Q));
    res->Add(NameValData("Comp_f", geo_f));
    res->Add(NameValData("Comp_Cd", geo_Cd));
    res->Add(NameValData("Comp_PercTotalCd", geo_percTotalCd));
    res->Add(NameValData("Comp_SurfNum", geo_surfNum));

    // Excres Related
    res->Add(NameValData("Num_Excres", (int)m_ExcresRowVec.size()));
    res->Add(NameValData("Excres_Label", excres_Label));
    res->Add(NameValData("Excres_Type", excres_Type));
    res->Add(NameValData("Excres_Input", excres_Input));
    res->Add(NameValData("Excres_Amount", excres_Amount));
    res->Add(NameValData("Excres_PercTotalCd", excres_PercTotalCd));

    // Totals
    res->Add(NameValData("Geom_f_Total", GetGeomfTotal()));
    res->Add(NameValData("Geom_Cd_Total", GetGeometryCd()));
    res->Add(NameValData("Geom_Perc_Total", GetGeomPercTotal()));
    res->Add(NameValData("Excres_f_Total", GetExcresfTotal()));
    res->Add(NameValData("Excres_Cd_Total", GetTotalExcresCD()));
    res->Add(NameValData("Excres_Perc_Total", GetExcresPercTotal()));
    res->Add(NameValData("Total_f_Total", GetfTotal()));
    res->Add(NameValData("Total_Cd_Total", GetTotalCD()));
    res->Add(NameValData("Total_Perc_Total", GetPercTotal()));

    string f_name = veh->getExportFileName(vsp::DRAG_BUILD_CSV_TYPE);
    res->WriteParasiteDragFile(f_name);

    return res->GetID();
}

string ParasiteDragMgrSingleton::ExportToCSV(const string & file_name)
{
    m_FileName = file_name;
    return ExportToCSV();
}

// ========================================================== //
// =================== General Methods ====================== //
// ========================================================== //

void ParasiteDragMgrSingleton::ClearInputVectors()
{
    geo_geomID.clear();
    geo_subsurfID.clear();
    geo_label.clear();
    geo_percLam.clear();
    geo_shapeType.clear();
    geo_ffIn.clear();
    geo_Q.clear();
    geo_Roughness.clear();
    geo_TeTwRatio.clear();
    geo_TawTwRatio.clear();
    geo_surfNum.clear();
    geo_expandedList.clear();
}

void ParasiteDragMgrSingleton::ClearOutputVectors()
{
    geo_groupedAncestorGen.clear();
    geo_swet.clear();
    geo_lref.clear();
    geo_Re.clear();
    geo_cf.clear();
    geo_fineRat.clear();
    geo_ffType.clear();
    geo_ffName.clear();
    geo_ffOut.clear();
    geo_f.clear();
    geo_Cd.clear();
    geo_percTotalCd.clear();
}

xmlNodePtr ParasiteDragMgrSingleton::EncodeXml(xmlNodePtr & node)
{
    char str[256];
    xmlNodePtr ParasiteDragnode = xmlNewChild(node, NULL, BAD_CAST"ParasiteDragMgr", NULL);

    ParmContainer::EncodeXml(ParasiteDragnode);
    XmlUtil::AddStringNode(ParasiteDragnode, "ReferenceGeomID", m_RefGeomID);

    xmlNodePtr ExcresDragnode = xmlNewChild(ParasiteDragnode, NULL, BAD_CAST"Excrescence", NULL);

    XmlUtil::AddIntNode(ExcresDragnode, "NumExcres", m_ExcresRowVec.size());

    for (size_t i = 0; i < m_ExcresRowVec.size(); ++i)
    {
        sprintf(str, "Excres_%i", i);
        xmlNodePtr excresqualnode = xmlNewChild(ExcresDragnode, NULL, BAD_CAST str, NULL);

        XmlUtil::AddStringNode(excresqualnode, "Label", m_ExcresRowVec[i].Label);
        XmlUtil::AddIntNode(excresqualnode, "Type", m_ExcresRowVec[i].Type);
        XmlUtil::AddDoubleNode(excresqualnode, "Input", m_ExcresRowVec[i].Input);
    }

    return ParasiteDragnode;
}

xmlNodePtr ParasiteDragMgrSingleton::DecodeXml(xmlNodePtr & node)
{
    char str[256];

    xmlNodePtr ParasiteDragnode = XmlUtil::GetNode(node, "ParasiteDragMgr", 0);

    int numExcres;
    if (ParasiteDragnode)
    {
        ParmContainer::DecodeXml(ParasiteDragnode);
        m_RefGeomID = XmlUtil::FindString(ParasiteDragnode, "ReferenceGeomID", m_RefGeomID);

        xmlNodePtr ExcresDragnode = XmlUtil::GetNode(ParasiteDragnode, "Excrescence", 0);

        numExcres = XmlUtil::FindInt(ExcresDragnode, "NumExcres", 0);

        for (int i = 0; i < numExcres; i++)
        {
            sprintf(str, "Excres_%i", i);
            xmlNodePtr excresqualnode = XmlUtil::GetNode(ExcresDragnode, str, 0);

            m_ExcresType.Set(XmlUtil::FindInt(excresqualnode, "Type", 0));
            m_ExcresValue.Set(XmlUtil::FindDouble(excresqualnode, "Input", 0.0));

            AddExcrescence();
        }
    }

    return ParasiteDragnode;
}

void ParasiteDragMgrSingleton::SortMap()
{
    SortMainTableVecByGroupedAncestorGeoms();
    switch (m_SortByFlag())
    {
    case PD_SORT_NONE:
        break;

    case PD_SORT_WETTED_AREA:
        SortMapByWettedArea();
        break;

    case PD_SORT_PERC_CD:
        SortMapByPercentageCD();
        break;

    default:
        break;
    }
}

void ParasiteDragMgrSingleton::SortMapByWettedArea()
{
    // Create New Map That Contains Wetted Area Value, Geom ID
    // Sort this Map
    // Recreate m_TableRowMap
    Vehicle* veh = VehicleMgr.GetVehicle();
    vector < ParasiteDragTableRow > temp;
    vector < bool > isSorted(m_TableRowVec.size(), false);
    double cur_max_ind = 0;
    int i = 0;

    while (!CheckAllTrue(isSorted))
    {
        if (!isSorted[i])
        {
            // Grabs Current Max Index Based on Unsorted Numbers
            cur_max_ind = i;
            for (size_t j = 0; j < m_TableRowVec.size(); ++j)
            {
                if (!isSorted[j])
                {
                    if (m_TableRowVec[j].Swet > m_TableRowVec[cur_max_ind].Swet)
                    {
                        cur_max_ind = j;
                    }
                }
            }
            isSorted[cur_max_ind] = true;
            temp.push_back(m_TableRowVec[cur_max_ind]);

            // Immediately pushes back any reflected surfaces behind last MAX
            for (size_t j = 0; j < m_TableRowVec.size(); ++j)
            {
                if (m_TableRowVec[cur_max_ind].GeomID.compare(m_TableRowVec[j].GeomID) == 0 && cur_max_ind != j && !isSorted[j])
                {
                    isSorted[j] = true;
                    temp.push_back(m_TableRowVec[j]);
                }
            }

            // Then pushes back any incorporated geoms behind reflected surfaces
            for (size_t j = 0; j < m_TableRowVec.size(); ++j)
            {
                Geom* geom = veh->FindGeom(m_TableRowVec[j].GeomID);
                if (geom)
                {
                    if (m_TableRowVec[cur_max_ind].GeomID.compare(geom->GetAncestorID(m_TableRowVec[j].GroupedAncestorGen)) == 0 && cur_max_ind != j && !isSorted[j])
                    {
                        isSorted[j] = true;
                        temp.push_back(m_TableRowVec[j]);
                    }
                }
            }
        }
        if (i != isSorted.size() - 1)
        {
            ++i;
        }
        else
        {
            i = 0;
        }
    }

    m_TableRowVec = temp;
}

void ParasiteDragMgrSingleton::SortMapByPercentageCD()
{
    vector < ParasiteDragTableRow > temp;
    vector < bool > isSorted(m_TableRowVec.size(), false);
    int cur_max_ind = 0;
    int i = 0;

    while (!CheckAllTrue(isSorted))
    {
        if (!isSorted[i])
        {
            // Grabs Current Max Index Based on Unsorted Numbers
            cur_max_ind = i;
            for (size_t j = 0; j < m_TableRowVec.size(); ++j)
            {
                if (!isSorted[j])
                {
                    if (m_TableRowVec[j].PercTotalCd > m_TableRowVec[cur_max_ind].PercTotalCd)
                    {
                        cur_max_ind = j;
                    }
                }
            }
            isSorted[cur_max_ind] = true;
            temp.push_back(m_TableRowVec[cur_max_ind]);

            // Immediately pushes back any reflected surfaces behind last MAX
            for (size_t j = 0; j < m_TableRowVec.size(); ++j)
            {
                if (m_TableRowVec[cur_max_ind].GeomID.compare(m_TableRowVec[j].GeomID) == 0 && cur_max_ind != j && !isSorted[j])
                {
                    isSorted[j] = true;
                    temp.push_back(m_TableRowVec[j]);
                }
            }

            // Then pushes back any incorporated geoms behind reflected surfaces
            for (size_t j = 0; j < m_TableRowVec.size(); ++j)
            {
                Geom* geom = VehicleMgr.GetVehicle()->FindGeom(m_TableRowVec[j].GeomID);
                if (geom)
                {
                    if (m_TableRowVec[cur_max_ind].GeomID.compare(geom->GetAncestorID(m_TableRowVec[j].GroupedAncestorGen)) == 0 && cur_max_ind != j && !isSorted[j])
                    {
                        isSorted[j] = true;
                        temp.push_back(m_TableRowVec[j]);
                    }
                }
            }
        }

        // Increase until max reached, then go back through to make sure everything is in its place
        if (i != isSorted.size() - 1)
        {
            ++i;
        }
        else
        {
            i = 0;
        }
    }

    m_TableRowVec = temp;
}

void ParasiteDragMgrSingleton::SortMainTableVecByGroupedAncestorGeoms()
{
    // Takes m_TableRowVec
    // For Each Geom ID Check if other geoms have it as an incorporated ID
    // If yes, place those below current geom
    Vehicle* veh = VehicleMgr.GetVehicle();
    vector < ParasiteDragTableRow > temp;
    string masterGeomID;
    vector < bool > isSorted(m_TableRowVec.size(), false);

    for (size_t i = 0; i < m_TableRowVec.size(); ++i)
    {
        if (!isSorted[i])
        {
            temp.push_back(m_TableRowVec[i]);
            isSorted[i] = true;

            // Immediately pushes back any reflected surfaces behind last TableRow
            for (size_t j = 0; j < m_TableRowVec.size(); ++j)
            {
                if (m_TableRowVec[i].GeomID.compare(m_TableRowVec[j].GeomID) == 0 && i != j && !isSorted[j])
                {
                    isSorted[j] = true;
                    temp.push_back(m_TableRowVec[j]);
                }
            }

            // Then pushes back any incorporated geoms behind reflected surfaces
            for (size_t j = 0; j < m_TableRowVec.size(); ++j)
            {
                masterGeomID = m_TableRowVec[i].GeomID;
                Geom* geom = veh->FindGeom(m_TableRowVec[j].GeomID);
                if (geom->GetAncestorID(m_TableRowVec[j].GroupedAncestorGen).compare(masterGeomID) == 0 && !isSorted[j])
                {
                    isSorted[j] = true;
                    temp.push_back(m_TableRowVec[j]);

                    // TODO push back geoms grouped to last grouped geom
                }
            }
        }
    }

    m_TableRowVec = temp;
}

bool ParasiteDragMgrSingleton::CheckAllTrue(vector<bool> vec)
{
    for (size_t i = 0; i < vec.size(); ++i)
    {
        if (!vec[i])
        {
            return false;
        }
    }
    return true;
}

void ParasiteDragMgrSingleton::DeactivateParms()
{
    m_Vinf.Deactivate();
    m_Hinf.Deactivate();
    m_Temp.Deactivate();
    m_DeltaT.Deactivate();
    m_Pres.Deactivate();
    m_Rho.Deactivate();
    m_SpecificHeatRatio.Deactivate();
    m_DynaVisc.Deactivate();
    m_KineVisc.Deactivate();
    //m_MediumType.Deactivate();
    m_Mach.Deactivate();
    m_ReqL.Deactivate();
}

bool ParasiteDragMgrSingleton::IsSameGeomSet()
{
    Vehicle* veh = VehicleMgr.GetVehicle();

    vector <string> newIDVec = veh->GetGeomSet(m_SetChoice());
    vector < string > newVecToCompare;
    for (int i = 0; i < newIDVec.size(); i++)
    {
        if (veh->FindGeom(newIDVec[i])->GetType().m_Type != MESH_GEOM_TYPE &&
            veh->FindGeom(newIDVec[i])->GetType().m_Type != BLANK_GEOM_TYPE &&
            veh->FindGeom(newIDVec[i])->GetType().m_Type != HINGE_GEOM_TYPE)
        {
            if (veh->FindGeom(newIDVec[i])->GetSurfPtr(0)->GetSurfType() != vsp::DISK_SURF)
            {
                newVecToCompare.push_back(newIDVec[i]);
            }
        }
    }

    int temprowsize = 0;
    for (int i = 0; i < newVecToCompare.size(); i++)
    {
        Geom* geom = veh->FindGeom(newVecToCompare[i]);
        if (geom)
        {
            temprowsize += geom->GetNumTotalSurfs();
            for (size_t j = 0; j < geom->GetSubSurfVec().size(); ++j)
            {
                for (size_t k = 0; k < geom->GetNumSymmCopies(); ++k)
                {
                    ++temprowsize;
                }
            }
        }
    }

    if (temprowsize == m_RowSize && newVecToCompare == m_PDGeomIDVec)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool ParasiteDragMgrSingleton::IsNotZeroLineItem(int index)
{
    // IF NOT subsurface
    // =============
    // 0th surface (e.g. WingGeom_0)
    // OR
    // List is Expanded
    // OR
    // is a custom geom (TODO could use work)
    // =============
    // AND
    // =============
    // Not grouped with ANY ancestors
    // OR
    // Any ancestors have expanded lists
    // OR
    // Has an expanded list
    // =============
    // Else 
    // =============
    // Sub surface is included in wetted area
    // AND
    // Geom has an expanded list

    // If incorporated, swet is added to choosen ancestor
    // Main surf will never be a zero line item
    // Custom types can have several geom types in them
    // If Geom list is expanded all components use their individual swets
    Vehicle* veh = VehicleMgr.GetVehicle();
    if (geo_subsurfID[index].compare("") == 0)
    {
        if (((geo_surfNum[index] == 0) ||
            (veh->FindGeom(geo_geomID[index])->m_ExpandedListFlag()) ||
            (geo_label[index].substr(0, 3).compare("[W]") == 0 || geo_label[index].substr(0, 3).compare("[B]") == 0)) &&
            (geo_groupedAncestorGen[index] == 0 ||
            (veh->FindGeom(veh->FindGeom(geo_geomID[index])->GetAncestorID(geo_groupedAncestorGen[index]))->m_ExpandedListFlag()) ||
                veh->FindGeom(geo_geomID[index])->m_ExpandedListFlag()))
        {
            return true;
        }
    }
    else
    {
        if (veh->FindGeom(geo_geomID[index])->GetSubSurf(geo_subsurfID[index])->m_IncludeFlag() &&
            veh->FindGeom(geo_geomID[index])->m_ExpandedListFlag())
        {
            return true;
        }
    }

    return false;
}

void ParasiteDragMgrSingleton::RefreshDegenGeom()
{    // Check For Similarity in Geom Set
    if (!IsSameGeomSet())
    {
        // Reset Geo Vecs & Clear Degen Geom
        VehicleMgr.GetVehicle()->ClearDegenGeom();
        m_DegenGeomVec.clear();
        ClearInputVectors();
        ClearOutputVectors();

        // Set New Active Geom Vec
        SetActiveGeomVec();
    }
}
