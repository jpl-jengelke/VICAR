//////////////////////////////////////////////////////////////////////////
// PigPHX
//
// Contains all mission-specific code for dealing 
// with the Mars Phoenix mission.
// 
////////////////////////////////////////////////////////////////////////

#include <stdlib.h>

#include "PigPHX.h"
#include "PigLabelModelPHX.h"
#include "PigFileModelPHX.h"
#include "PigCAHVOR.h"
#include "PigCAHVORE.h"
#include "PigPointGenericCamera_0DOF.h"
#include "PigPointGenericCamera_AzEl.h"
#include "PigPointPHXssiCamera.h"
#include "PigPointPHXssiCamera3dof.h"
#include "PigPointPHXracCamera.h"
#include "PigPointPHXracCamera6dof.h"
#include "PigRoverStateManagerPHX.h"
#include "RadiometryPHX.h"
#include "PigCSReference.h"
#include "PigConfigMgrTempPHX.h"

#include "PigXerces.h"

PigRoverStateManager *PigPHX::_phx_rsm = NULL;

PigCoordSystem *PigPHX::_phx_cs_db[PIG_MAX_CS];
int PigPHX::_phx_cs_db_count = 0;
PigCoordSystem *PigPHX::_phx_fixed_cs = NULL;

////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////
PigPHX::PigPHX(const char *mission_name, const char *host_id)
{
    strcpy(_mission_name, mission_name);
    strcpy(_host_id, host_id);
}

////////////////////////////////////////////////////////////////////////
// Create PHX camera model given an image file.
////////////////////////////////////////////////////////////////////////

PigCameraModel *PigPHX::createCameraModel(PigFileModel *file,
                                          const char *special)
{
    int count;
    char *cm_name = NULL;
    char point_method[256];
    char geom_proj_type[11];
    PigCoordSystem *cs = getCoordSystem(file, NULL);
    PigCameraModel *model = NULL;

    // If there's no geometry projection type then it's not a normally-labeled
    // file... so there's no camera model to create.
    // TBD: create a mosaic camera model if that's what it is... !!!!

    const char *p = file->getGeometryProjectionType();
    if (p == NULL)
	return NULL;
    strcpy(geom_proj_type, p);

    int dx = file->getFirstLineSample(1) - 1;
    int dy = file->getFirstLine(1) - 1;
    int hscale = (int)(file->getDownsampleXFactor(1.0));
    int vscale = (int)(file->getDownsampleYFactor(1.0));


    // Get Instrument ID
    const char *inst_id = file->getInstrumentId();

    // make sure that frame id is valid given inst_id name
    const char *frame = file->getFrameId();
    bool match = TRUE;

    if(strstr(inst_id, "RIGHT") != NULL) {
        if(frame == NULL && frame[0] != 'R')
            match = FALSE;
    }
    else if(strstr(inst_id, "LEFT") != NULL) {
        if(frame == NULL && frame[0] != 'L')
                        match = FALSE;
    }
        
    if(!match)
        PigModelBase::printStaticMsg(
        "WARNING: FrameId does not match InstrumentId name or Unrecognized FrameId!", PigMsgWarning);

    // Put subframe and downsampling info into the 
    // construction string
    char sub_down_str[150];
    sprintf(sub_down_str, "kinematics type=%s subframe=(%d,%d) downsampling=(%d,%d) temp=%f", 
	    geom_proj_type, dx, dy, hscale, vscale,
	    file->getInstrumentOpticsTemperature(0.0));

    // construct subtype string based on instrument type
    char *subtype = NULL;
    if(strncasecmp(inst_id,"SSI",3) == 0)
        subtype = strdup(file->getFilterNumber());
    else if(strncasecmp(inst_id, "RAC", 3) == 0) {
        //0-pad it to 3-digit
        subtype = new char[4];
        sprintf(subtype, "%03d", file->getInstrumentFocalLengthCount(0));
    }

    // Get the camera model name to use
    getParam("POINT_METHOD", point_method, &count, 1, 0);

    char *pointMethodValue = NULL;
    if (count == 1) {   
        pointMethodValue = parseParamString(point_method, "cm");
        if(pointMethodValue != NULL && 
           (strncasecmp(pointMethodValue, "label",5) == 0)) {
            
            cm_name = parseParamString(point_method, "CM_NAME");
            
            // Look for labels
            
            file->openFile();                       // just to make sure
                        
            // Named labels
                        
            if (model == NULL) {
                if (cm_name != NULL)
                    model = PigCameraModel::createFromLabel(file, // file model
                                 cm_name,                       // instance
                                 getMissionName(),              // mission 
                                 inst_id,                       // instrument 
                                 NULL,                          // version
                                 subtype,                       // subtype
                                 "label",                       // construction
                                 file->getCalibrationSourceId(),// calibration
                                 cs);                           // coord_sys 
            }
                        
            // Instance 1
                        
            if (model == NULL) {
                model = PigCameraModel::createFromLabel(file,   // file model
                                1,                              // instance
                                getMissionName(),               // mission 
                                inst_id,                        // instrument 
                                NULL,                           // version
                                subtype,                        // subtype
                                "label",                        // construction
			        file->getCalibrationSourceId(), // calibration
                                cs);                            // coord_sys 
            }

            if (model != NULL)
                return model;
        }
        else if(pointMethodValue != NULL && 
                (strncasecmp(pointMethodValue, "kinematics", 10) == 0)) {
            model =  createCameraModel(inst_id,                 // instrument
                                NULL,                           // version
                                subtype,                        // subtype  
                                special,                        // special
                                sub_down_str,                   // construction
				file->getCalibrationSourceId(), // calibration
				cs);                            // coord_sys
        }
    }
    
    //If point_method's cm  parameter is not specified, or the value is not 
    //recognized as a valid input, assume "kinematics" subtype as a default.
    if (!model)
        model = createCameraModel(inst_id,                      // instrument
                                NULL,                           // version
                                subtype,                        // subtype
                                special,                        // special
                                sub_down_str,                   // construction
				file->getCalibrationSourceId(), // calibration
				cs);                            // coord_sys
        
    // free memory
    delete[] subtype;
    return model;
}

////////////////////////////////////////////////////////////////////////
// Create PHX camera model given strings.  Valid strings are:
// instrument:  "SSI Left", "SSI Right", "RAC", "OM"
//       
// version: Serial Number of the camera, from the INSTRUMENT_SERIAL_NUMBER
//          label.  
// subtype: Filter number or Focus position.  For cameras like PHX SSI, the value
//          of FILTER_NUMBER label. For cameras like PHX RAC, the value
//          ArticulationDeviceCount in RAC_Articulation_State prop group.
// special: standard string (across all subclasses).  "stereo" will return
//          the "stereo partner" of the model instead.
// construction:   How camera model is derived.
// calibration_id: The value of the CALIBRATION_SOURCE_ID in the Camera Model
//                 label.
// type: Type of camera model, like cahv, cahvor or cahvore  
//
// All strings are case-insensitive.
//
// If the PigCoordSystem is NULL, the "natural" frame for the given
// mission/instrument, at the default site, are used.
////////////////////////////////////////////////////////////////////////

PigCameraModel *PigPHX::createCameraModel(const char *instrument,
                                          const char *version,
                                          const char *subtype, 
                                          const char *special,
                                          const char *construction, 
                                          const char *calibration, 
                                          PigCoordSystem *cs)
{
    char msg[1024];
    int stereo;
    int status;
    PigCameraModel *model = NULL;
    PigCameraMapper *map = NULL;
    PigCameraMapEntry *entry = NULL;

    if (cs == NULL)                            // use default, natural frame
        cs = getCoordSystem(getNaturalFrame(instrument));
    else
        cs = getCoordSystem(cs, getNaturalFrame(instrument));


    // Check the "special" string
    stereo = 0;
    if (special != NULL && strlen(special) > 0) {
        if (strcasecmp(special, "stereo") == 0)
            stereo = 1;
        else
            return NULL;                // unknown special string
    }

    // Find out the name of camera model file
    PigXerces::initialize();

    map = new PigCameraMapper(getMissionName(), getHostID());
    
    if (!map)
        return NULL;  // can't create mapper for given hostid 

    if (stereo) {               // swap instrument names
        entry = map->findFromStereoPartnerID(instrument);
    }
    else {
        // Having Version is a rare case, usually it's null
        if (version) {
            entry = map->findFromSerialNumber(version);
            // check for consistency with instrument id
            if(!strcmp(entry->getName(), instrument)) {
                sprintf(msg, "Warning PigCameraMapEntry.getName=%s, does not match instrument_name= %s",
                        entry->getName(), instrument);
		PigModelBase::printStaticMsg(msg, PigMsgWarning);
            }
              
        }

        // This is the most common case: based on instrument name
        // we find the map entry.
        else if(instrument)
            entry = map->findFromID(instrument);
    } 

    if (entry) {
        // We create an instance now then fill in the parameters later.
        model = createCameraModel(entry, subtype, construction, calibration);
	
	if(!model)
	    return NULL;
    }
    else {
        // No Entry Found!
        sprintf(msg, 
                "No Entry Found in Camera Mapping XML file for PHX Host_Id: %s, instrument: %s", 
                getHostID(),instrument);
        PigModelBase::printStaticMsg(msg, PigMsgFatal);
        return NULL;
    }
    
    char *filter = NULL;
    
    model->setInitialCoordSystem(cs);
    
    // This is not very clean, since we make assumption that
    // instrument may have either filter wheel OR focus adjustment
    // Works for PHX, MUST be changed for MSL which will have
    // cameras that have both
    if ((entry->getFilters() || entry->getFocus()) && subtype) {
        filter = strdup(subtype);
    }
    
    status = getCameraModelParmsFromFile(model,
					 cs,
					 calibration,
					 entry->getSerialNumber(),
					 filter,
					 entry->getType());
 
    if (!status)
        return NULL;   // We failed to read camera model's parms data from file

    return adjustCameraModel(model, cs, map, entry, calibration,
			     construction, filter);
}

//////////////////////////////////////////////////////////////////////
// Construct the string of calibration camera model filename
// Here we allow any of the strings to be NULL.
// Returns status: SUCESS = 1, FAIL = 0
//////////////////////////////////////////////////////////////////////
int PigPHX::getCameraModelParmsFromFile(PigCameraModel *model,
					PigCoordSystem *cs,
					const char *calibration,
					const char *serial,
					const char *filter,
					const char *type)
{
  int status = 1;
  char cmod_file[PIG_MAX_FILENAME_SIZE+1];

    // First we attempt to find camera model cal. file
    // using "serial", "filter" and "type"
  constructCameraModelFileName(calibration, serial, 
			       filter, type, cmod_file);

    // This should succeed.  If it doesn't, we have a big
    // reason for concern.
    status = readCameraModelFromFile(model, cs, cmod_file);
 
    if (status == 1)  // success reading the file
        return status;

    // BACK-UP: Try to find calibration camera model by discarding filter info
    constructCameraModelFileName(calibration, serial, NULL, 
				 type, cmod_file);

    status = readCameraModelFromFile(model, cs, cmod_file);
 
    if (!status)
        return status;

    // if we are here, all our attempts to find calibration
    // camera model files have failed. 
    PigModelBase::printStaticMsg("Unable to find calibration camera model file!", 
                                 PigMsgFatal);
    return status; 

}
////////////////////////////////////////////////////////////////////////
// Given strings constructs full-path filename
// Note that calibration currently is not used to construct filename.
// Returns cmod_file char string.
////////////////////////////////////////////////////////////////////////
void PigPHX::constructCameraModelFileName(const char *calibration,
					  const char *serial,
					  const char *filter,
					  const char *type,
					  char *cmod_file)
{    
    strcpy(cmod_file, "");
    strcat(cmod_file, "camera_models/PHX");
    
    if (serial) {
        strcat(cmod_file, "_SN_");
        strcat(cmod_file, serial);
    }

    if (filter) {
        strcat(cmod_file, "_F_");
	if (strlen(filter) == 1)
	    strcat(cmod_file, "0");	// 0-pad 1-digit numbers
        strcat(cmod_file, filter);
    }

    if (type) {
        strcat(cmod_file, ".");
        strcat(cmod_file, type);
    }
}

///////////////////////////////////////////////////////////////
// Given filename string(cmod_file) read camera model from that
// Returns status: SUCESS = 1, FAIL = 0
///////////////////////////////////////////////////////////////
int PigPHX::readCameraModelFromFile(PigCameraModel *model,
				    PigCoordSystem *cs,
				    char *cmod_file)
{
    char cmod_path[1024];
    int status = 1;

    if (!model || !cs)  // check for NULL values
        return 0;


    FILE *f = PigModelBase::openConfigFile(cmod_file, cmod_path);
    if (f != NULL) {
        fclose(f);
        status = model->readFromFile(cmod_path, cs);
    }
  
        char msg[1024];  
    if (f == NULL || status != 0) {             // file not present
        sprintf(msg,
                "Unable to find calibration camera model file: %s (path=%s)",
                cmod_file, cmod_path);
        PigModelBase::printStaticMsg(msg, PigMsgWarning);
	return 0;
    }
    else {
        sprintf(msg,
                "Successfully read calibration camera model from file: %s (path=%s)",
                cmod_file, cmod_path);
        PigModelBase::printStaticMsg(msg, PigMsgInfo);
	return 1;
    }
}

//////////////////////////////////////////////////////////////////////
// Creates camera model given PigCameraMapEntry and strings 
// the value of which can't be derived from PigCameraMapEntry instance
//////////////////////////////////////////////////////////////////////
PigCameraModel *PigPHX::createCameraModel(PigCameraMapEntry *entry,
					  const char *filter,
					  const char *construction,
					  const char *calibration)
{
    // SSI and RAC cameras use CAHVOR by default, 
    // while OM is telecentric 
    // We create an instance now then fill in the parameters.

    char msg[1024];
    char type[7];

    if (!entry)
      return NULL;

    strcpy(type, entry->getType());

    if(strncasecmp(type, "CAHVOR", 6) == 0)
      return new PigCAHVOR(getMissionName(), 
			     entry->getID(), entry->getSerialNumber(), 
			     filter, construction, calibration);
    else if(strncasecmp(type, "CAHV", 4) == 0)
      return new PigCAHV(getMissionName(), 
			     entry->getID(), entry->getSerialNumber(), 
			     filter, construction, calibration);
    else {
        PigModelBase::printStaticMsg("Camera Model does not match any known type, revert to default type for known instruments", 
				     PigMsgWarning);
	// Get the default type for known instruments
	if((strncasecmp(entry->getShortID(), "S", 1) == 0) ||
	   (strncasecmp(entry->getShortID(), "R", 1) == 0)) {
	   
	    return new PigCAHVOR(getMissionName(), 
				 entry->getID(), entry->getSerialNumber(), 
				 filter, construction, calibration);
	}
	/* handle telecentric case for OM
	else if(strncasecmp(entry->getShortID(), "H", 1) == 0) {
	    return new PigCAHVORE(getMissionName(), 
				  entry->getID(), entry->getSerialNumber(), 
				  filter, construction, calibration);
	}
	*/
	else {
	  sprintf(msg, "Unknown PHX instrument %s", entry->getShortID());
	  PigModelBase::printStaticMsg(msg, PigMsgFatal);
	  return NULL;
	}
    }
}

////////////////////////////////////////////////////////////////////////
// Using the info in construction string, modify input camera model
// for "Linearized", "Subframed", "Downsampled" images 
// Also takes care of geometric temperature compensation.
////////////////////////////////////////////////////////////////////////

PigCameraModel* PigPHX::adjustCameraModel(PigCameraModel *camera_in,
					  PigCoordSystem *cs,
					  PigCameraMapper *map,
					  PigCameraMapEntry *entry,
					  const char *calibration,
					  const char *construction,
					  const char *filter)
{
    // Adjust the model for subframing and/or downsampling 
    // and/or linearized type.
    int dx, dy, hscale, vscale;
    char cmod_file[PIG_MAX_FILENAME_SIZE+1];
    PigCameraMapEntry *partner_entry = NULL;
    PigCameraModel *partner_cm = NULL;
    PigCameraModel *camera_out = NULL;

    // set defaults
    dx = dy = 0;
    hscale = vscale = 1;
    int use_temp = FALSE;
    float temperature = 0.0;

    if (!camera_in)
        return NULL;

    // Read the construction string
    char proj_type[11];
    strcpy(proj_type, "");
    if (strstr(construction, "temp=") != NULL) {
        sscanf(construction,
	    "kinematics type=%s subframe=(%d,%d) downsampling=(%d,%d) temp=%f",
	       &proj_type, &dx, &dy, &hscale, &vscale, &temperature);
	use_temp = TRUE;
    } else {
        sscanf(construction,
	    "kinematics type=%s subframe=(%d,%d) downsampling=(%d,%d)",
	   &proj_type, &dx, &dy, &hscale, &vscale);
    }

    if (!strcmp(proj_type, "LINEARIZED")) {
        if (map)
	    partner_entry = map->findStereoPartner(entry);

	if (partner_entry) {
	    constructCameraModelFileName(NULL, 
					 partner_entry->getSerialNumber(),
					 filter,
					 partner_entry->getType(),
					 cmod_file);

	    partner_cm = createCameraModel(partner_entry,
					   filter,
					   construction,
					   calibration);
	    if (partner_cm) {
	        partner_cm->setInitialCoordSystem(cs);

		if (!readCameraModelFromFile(partner_cm, cs, cmod_file))
	            partner_cm = NULL;
	    }
	}

	camera_out = camera_in->alignStereoCameras(NS_PHX, NL_PHX,
						   NS_PHX, NL_PHX,
						   NS_PHX, NL_PHX,
						   partner_cm);       
    }
    else {
        // for "RAW" output camera is the same as an input
        camera_out = camera_in;
    }
    
    // Temperature-dependent geometric correction.  SSI only.

    const char *instrument = camera_out->getInstrumentName();
    if (use_temp && (strncasecmp(instrument, "SSI", 3) == 0)) {

	PigConfigMgrTempPHX *params = PigConfigMgrTempPHX::getInstance(
		getMissionName(), getHostID(), instrument, filter);

	// Pointing compensation

	double x_pixels = params->_align_x[0] +
			  params->_align_x[1] * temperature +
			  params->_align_x[2] * temperature * temperature;

	double y_pixels = params->_align_y[0] +
			  params->_align_y[1] * temperature +
			  params->_align_y[2] * temperature * temperature;

	double x_rad = x_pixels * params->_mrad_per_pixel / 1000.0;
	double y_rad = y_pixels * params->_mrad_per_pixel / 1000.0;

	// Rotate camera by getting current az/el of the A vector for an
	// initial quat, and adding the above deltas to it for the final
	// quat.  We rotate around C.

	PigVector y_axis(0.0, 1.0, 0.0);
	PigVector z_axis(0.0, 0.0, 1.0);

	double az = cs->getAz(camera_out->getCameraOrientation());
	double el = cs->getEl(camera_out->getCameraOrientation());

	PigQuaternion rot1 = PigQuaternion(z_axis, az) *
			     PigQuaternion(y_axis, el);
	PigQuaternion rot2 = PigQuaternion(z_axis, az + x_rad) *
			     PigQuaternion(y_axis, el + y_rad);
	camera_out->moveCamera(camera_out->getCameraPosition(), rot1,
			       camera_out->getCameraPosition(), rot2, cs);
        camera_out->setInitialFromCurrent();

	// Now scale camera

	double x_scale = params->_scale_x[0] +
			 params->_scale_x[1] * temperature +
			 params->_scale_x[2] * temperature * temperature;

	double y_scale = params->_scale_y[0] +
			 params->_scale_y[1] * temperature +
			 params->_scale_y[2] * temperature * temperature;

        camera_out->scaleCamera(1.0/(1.0 + x_scale), 1.0/(1.0 + y_scale));
        camera_out->setInitialFromCurrent();

    }

    // Note that both shiftCamera() and shiftScale() modify the
    // *current* set of CM parameters.  We must copy these to
    // the *initial* set so they can be the "calibration" model
    // for pointing.

    // shift the camera_out parameters for any subframing.
    // if both dx and dy are 0 we don't need a shift
    if ((dx > 0) || (dy > 0)) {
        camera_out->shiftCamera(dx, dy);
        camera_out->setInitialFromCurrent();
    }
    
    // scale the camera_out parameters for any downsampling
    // similarly to the above: don't need a scale if either
    // parameter is equal to 0 or both equal to 1
    if ((hscale != 0) && (vscale != 0) && ((hscale != 1) || (vscale != 1))) {
        camera_out->scaleCamera(1.0/hscale, 1.0/vscale);
        camera_out->setInitialFromCurrent();
    }

    return camera_out;
}        

////////////////////////////////////////////////////////////////////////
// Create an PHX pointing model given strings.  Valid strings are:
// instrument:  "SSI Left/Right", "RAC", "OM"
// All strings are case-insensitive.
// User-specified pointing method is stored in CameraModel's subtype member
// variable.  The following pointing methods supported for PHX:
//
// CM=KINEMATICS(default) 
// Uses dynamically derived camera models. 
// For mast-arm based instruments use physical pointing models(based on joint 
// angles), for rover mounted instruments use body fixed model.
// CM=LABEL 
// Uses camera models from the label.  For mast-arm based instruments
// use 2DOF AzEl GenericCamera pointing model.  Body Fixed cameras will use 
// 0DOF GenericCamera pointing model.
// CM=AzEl 
// Uses dynamically derived camera models. For mast-arm based 
// instruments use 2DOF AzEl GenericCamera pointing model.  Body Fixed cameras 
// will use 0DOF GenericCamera pointing model. 
// PHX=Unconstrained  
// Pointing model is always 0DOF
// 
////////////////////////////////////////////////////////////////////////

PigPointingModel *PigPHX::createPointingModel(PigCameraModel *cm, const char *instrument,
                                              const char *type, bool allow_type_override)
{
    int count;
    PigPointingModel *model;
    char msg[1024];
    char point_method[256];
    char *pointMethodValue = NULL;
    char model_type[30];

    if (type != NULL)
        strcpy(model_type, type);
    else
        strcpy(model_type, "");


    if (cm == NULL || instrument == NULL)
        return NULL;

    // Get the pointing model name to use
    getParam("POINT_METHOD", point_method, &count, 1, 0);

    if (count == 1)
        pointMethodValue = parseParamString(point_method, "pm");
    

    if (strncasecmp(instrument, "SSI", 3) == 0) {
       if (cm->getCameraConstruction() != NULL &&
            ((strncasecmp(cm->getCameraConstruction(), "label", 5) == 0)))
            strcpy(model_type, "GenericCamera_AzEl");

        else if (allow_type_override && pointMethodValue != NULL) {
            if (strncasecmp(pointMethodValue, "2dof", 4) == 0) {
                if (type != NULL && strcasecmp(type, "PHXssiCamera"))
                    PigModelBase::printStaticMsg("WARNING: Pointing Model mismatch: creating PHXssiCamera as requested by command line argument....",
                                                 PigMsgWarning);

                strcpy(model_type, "PHXssiCamera");
            }
            else if ((strcasecmp(pointMethodValue, "3dof") == 0)) {
                if (type != NULL && strcasecmp(type, "PHXssiCamera3dof"))
                    PigModelBase::printStaticMsg("WARNING: Pointing Model mismatch: creating PHXssiCamera3dof as requested by command line argument....",
                                                 PigMsgWarning);
                strcpy(model_type, "PHXssiCamera3dof");
            }
        }

        if (strcasecmp(model_type, "GenericCamera_AzEl") == 0)
            return new PigPointGenericCamera_AzEl(cm, this, NULL);

        // Az/El + twist pointing model
        else if (strcasecmp(model_type, "PHXssiCamera3dof") == 0)
            return new PigPointPHXssiCamera3dof(cm, this, instrument);
        else
            // Default 2dof(Az/El) pointing model
            return new PigPointPHXssiCamera(cm, this, instrument);

    }
    // !!!!Default should be 6dof but for now make default 7dof since it's a simpler model
    else if (strncasecmp(instrument, "RAC", 3) == 0) {
        if (cm->getCameraConstruction() != NULL &&
	    ((strncasecmp(cm->getCameraConstruction(), "label", 5) == 0)))
	    // !!!! should be changed to 6 dof model
            return new PigPointGenericCamera_AzEl(cm, this, NULL);
	else if ((pointMethodValue != NULL && allow_type_override &&  (strncasecmp(pointMethodValue, "6dof",4) == 0)) ||
	    (type != NULL &&  (strcasecmp(type, "PHXracCamera6dof") == 0)))
    	     return new PigPointPHXracCamera6dof(cm, this,instrument);
        else 
	    return new PigPointPHXracCamera(cm, this, instrument);
    }

    else {
        sprintf(msg, "Unknown PHX instrument %s", instrument);
        PigModelBase::printStaticMsg(msg, PigMsgFatal);
        return NULL;
    }
    // shouldn't be here...
    return NULL;
}

////////////////////////////////////////////////////////////////////////
// Create a PHX radiometry model given an image file. 
////////////////////////////////////////////////////////////////////////

RadiometryModel *PigPHX::createRadiometryModel(PigFileModel *file)
{
  return RadiometryPHX::create(file);
}

////////////////////////////////////////////////////////////////////////
// Create a PHX LabelModel.
////////////////////////////////////////////////////////////////////////

PigLabelModel *PigPHX::createLabelModel(int unit)
{
    return new PigLabelModelPHX(unit, getMissionName());
}

////////////////////////////////////////////////////////////////////////
// Create a PHX FileModel.
////////////////////////////////////////////////////////////////////////

PigFileModel *PigPHX::createFileModel(const char *filename, int unit)
{
    return new PigFileModelPHX(filename, unit, getMissionName());
}

////////////////////////////////////////////////////////////////////////
// Return the borders appropriate for the instrument type.
// This should only be called by PigFileModel!!
////////////////////////////////////////////////////////////////////////

int PigPHX::getBorders(PigFileModel *file, int &sl, int &ss,
                                           int &el, int &es)
{
    sl = 1;
    ss = 1;
    el = sl + file->getNL() - 2;
    es = ss + file->getNS() - 2;
    return TRUE;
}



////////////////////////////////////////////////////////////////////////
// Canonicalize a frame name and get the # of indices it should have.
// The returned strings are pointers to internal strings and MUST NOT
// be modified!
// Strings that must be supported for multimission compatibility:
//     FIXED - fixed frame
//     INSTRUMENT - the "natural" frame for the instrument
//     SITE
//     ROVER
//     LOCAL_LEVEL
// inst_id is used only if the frame is INSTRUMENT and can be NULL
// (if it's NULL and frame is INSTRUMENT, just get the "most common")
// Return 1 for success, 0 for frame not recognized (in which case it's
// as if FIXED was given)
// mask_indices (output) will contain 1 for relevant indices and -1 for
// those that should be wildcarded.  Valid pointer must be provided, or
// NULL for dont-care.  Mask will be filled up to max_indices.
////////////////////////////////////////////////////////////////////////

int PigPHX::canonicalizeFrameName(const char *frame, const char *inst_id,
                char *&canon_frame, char *&short_frame,
                int &max_indices, int *mask_indices)
{
    if (frame == NULL) {
        printError("NULL frame name given, ignored");
        canon_frame = "SITE_FRAME";     // same as FIXED
        short_frame = "SITE";
        max_indices = 0;
        if (mask_indices)
            mask_indices[0] = 1;
        return 0;
    }

    // Special handling of INSTRUMENT

    if (strcasecmp(frame, "INSTRUMENT") == 0)
        frame = getNaturalFrame(inst_id);

    // FIXED is special

    if ((strcasecmp(frame, "FIXED") == 0) ||
        (strcasecmp(frame, "MFX") == 0) ||              // alias
        (strcasecmp(frame, "SFX") == 0) ||
        (strcasecmp(frame, "Fixed_Frame") == 0) ||
        (strcasecmp(frame, "Surface Fixed") == 0)) {    // alias

        canon_frame = "FIXED_FRAME";
        short_frame = "FIXED";
        max_indices = 0;
        if (mask_indices)
            mask_indices[0] = 1;
        return 1;
    }

    if ((strcasecmp(frame, "Site") == 0) ||
        (strcasecmp(frame, "SITE_FRAME") == 0) ||
        (strcasecmp(frame, "Major Site") == 0)) {

        canon_frame = "SITE_FRAME";
        short_frame = "SITE";
        max_indices = 1;
        if (mask_indices)
            mask_indices[0] = 1;
        return 1;
    }

    if ((strcasecmp(frame, "MLL") == 0) ||              // alias
        (strcasecmp(frame, "LL") == 0) ||
        (strcasecmp(frame, "Mars Local Level") == 0) ||
        (strcasecmp(frame, "Local Level") == 0) ||
        (strcasecmp(frame, "LOCAL_LEVEL_FRAME") == 0) ||
        (strcasecmp(frame, "Local_Level") == 0)) {      // alias

        canon_frame = "LOCAL_LEVEL_FRAME";
        short_frame = "LL";
        max_indices = 2;
        if (mask_indices) {
            mask_indices[0] = 1;
            mask_indices[1] = 1;
        }
        return 1;
    }

    if ((strcasecmp(frame, "Rover") == 0) ||
        (strcasecmp(frame, "LANDER") == 0) ||
        (strcasecmp(frame, "LANDER_FRAME") == 0) ||
	(strcasecmp(frame, "Payload") == 0) ||
	(strcasecmp(frame, "PAYLOAD_FRAME") == 0)) {

        canon_frame = "PAYLOAD_FRAME";
        short_frame = "PAYLOAD";
        max_indices = 2;
        if (mask_indices) {
            mask_indices[0] = 1;
            mask_indices[1] = 1;
        }
        return 1;
    }

    if ((strcasecmp(frame, "OM") == 0) ||
        (strcasecmp(frame, "OM_FRAME") == 0)) {

	canon_frame = "OM_FRAME";
	short_frame = "OM";
	max_indices = 2;
        if (mask_indices) {
            mask_indices[0] = 1;
            mask_indices[1] = 1;
        }
        return 1;
    }

    char msg[256];
    sprintf(msg, "Unknown PHX coordinate system frame: '%s', Ignored", frame);
    printError(msg);

    canon_frame = "SITE_FRAME"; // same as FIXED
    short_frame = "SITE";
    max_indices = 1;
    if (mask_indices)
        mask_indices[0] = 1;

    return 0;
}

////////////////////////////////////////////////////////////////////////
// Return the RMC indices for this mission.
////////////////////////////////////////////////////////////////////////

const char *PigPHX::getRmcIndexName(int i)
{
    if (i == 0)  return "SITE";
    if (i == 1)  return "DRIVE";

    return "UNKNOWN";   // oops!
}

////////////////////////////////////////////////////////////////////////
// Get the PigRoverStateManager to use for this mission.  It is a singleton
// per mission.
////////////////////////////////////////////////////////////////////////

PigRoverStateManager *PigPHX::getRoverStateManager()
{
    if (_phx_rsm == NULL) {
        _phx_rsm = new PigRoverStateManagerPHX(this);
        // Add "telemetry" as the lowest priority...
        _phx_rsm->addPriority("telemetry");
    }
    return _phx_rsm;
}
