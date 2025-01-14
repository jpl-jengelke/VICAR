#include <stdio.h>
#include "PigPolarProjectionJ.h"
#include "PigMICAParamsJ.h"
#include "PigPoint2D_C.h"
#include "PigFileModel.h"
#include "PigCameraModel.h"
#include "PigPointingModel.h"
#include "PigMission.h"
#include "PigRoverStateManager.h"
#include "PigSurfaceModel.h"
#include "PigSurfacePlane.h"
#include "PigSurfaceInfinity.h"
#include "PigCoordSystem.h"
#include "mars_support.h"

#include "jpl_mipl_mica_pig_CameraModel.h"
#include "jpl_mipl_mica_projection_PolarProjection.h"

#include "mica_common.h"

extern "C" {
  /* -=================================================================- */
  /* -=================================================================- */
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    create
   * Signature: ()V
   */
  JNIEXPORT void JNICALL Java_jpl_mipl_mica_projection_PolarProjection_createNtv
  (JNIEnv *env, jobject obj, jdouble scale)
  {
    jclass clazz = 0;
    jfieldID fid = 0;

    printf ("here is polar createNtv\n");
    PigPolarProjection *proj = new PigPolarProjection (scale);

    if (proj == 0) {
      clazz = env->FindClass ("java/lang/InstantiationException");
      env->ThrowNew (clazz, "initialize: could not create a PigPolarProjection");
      return ;
    }
    // at this point you have a PigPolarProjection object.
    
    // now locate the field called \"cpp\", it's a java long 
    
    if ((getField (env, obj, &clazz, &fid, "cpp", "J")) != 1) {
      return ;
    }
    // now we stuff the address of the c++ object into the java proxy. 
    env->SetLongField (obj, fid, (long)proj);

    //printf ("scale = %.6f, origin = %.3f, %.3f, %.3f\n", proj->getScale(), proj->getOrigin()->getX(), proj->getOrigin()->getY(), proj->getOrigin()->getZ());
    return ;
    
  }
  /* -=================================================================- */
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    getAzFirstSample
   * Signature: ()D
   */
  JNIEXPORT jdouble JNICALL Java_jpl_mipl_mica_projection_PolarProjection_getAzFirstSample
  (JNIEnv *env, jobject obj)
  {
    PigPolarProjection *proj;
    
    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return 0;
 
    return proj->getAzFirstSample ();
  }
  /* -=================================================================- */
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    getFromLookVector
   * Signature: (Ljpl/mipl/mica/pig/SurfaceModel;Ljpl/mipl/mica/pig/PigVector;Ljpl/mipl/mica/pig/PigPoint;Ljpl/mipl/mica/pig/CoordSystem;)Ljpl/mipl/mica/pig/PigPoint2D;
   */
  JNIEXPORT jobject JNICALL Java_jpl_mipl_mica_projection_PolarProjection_getFromLookVector
  (JNIEnv *env, jobject obj, jobject SurfModel, jobject LookVect, jobject CamOrg, jobject CoordS)
  {
    PigPolarProjection       *proj;
    PigSurfaceModel          *sm;
    PigVector                *look;
    PigVector                *co;
    PigCoordSystem           *cs;
    jclass                   clazz;
    jclass                   pointClazz=0;
    jmethodID                mid=0;
    jobject                  newPoint=0;
    PigPoint2D               *fp;
    double                   new_az;
    int                      infinity;
    double                   pi2Radians = 2 * M_PI;

    // starting...
    // first we get a bunch of c++ pointers
    // the projection pointer, the look vector, the Camera origin, the Coordinate System
    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return 0;

    sm = (PigSurfaceModel*)resolveField (env, SurfModel);
    if (sm == 0)
      return 0;

    look = (PigVector*)resolveField (env, LookVect);
    if (look == 0)
      return 0;

    co = (PigVector*)resolveField (env, CamOrg);
    if (co == 0)
      return 0;

    cs = (PigCoordSystem*)resolveField (env, CoordS);
    if (cs == 0)
      return 0;

    // does the look vector intersect our surface?
    PigPoint *surf=new PigPoint(0.0, 0.0, 0.0); 
    int hits=sm->intersectRay(*co, *look, *surf);
    
    if (hits <= 0)
      infinity = 1;
    else
      infinity = 0;
    //printf ("1 surf = [ %.3f %.3f %.3f] \nlook = [%.3f %.3f %.3f ]\n", surf->getX(), surf->getY(), surf->getZ(), look->getX(), look->getY(), look->getZ());
    sm->getRay(*proj->_origin, *surf, infinity, *look);
    //printf ("2 surf = [ %.3f %.3f %.3f] look = [%.3f %.3f %.3f ]\n", surf->getX(), surf->getY(), surf->getZ(), look->getX(), look->getY(), look->getZ());

    double out_az = proj->_pi_half_radians - (cs->getAz(*look) - proj->_up_azimuth) * cs->getAzimuthDirection();
    
    double out_range = (cs->getEl(*look) + proj->_pi_half_radians) / proj->_scale;
    
    double rc = out_range ;
    //printf ("out_az = %.3f out_range = %.3f rc = %.3f\n", out_az, out_range, rc);

    PigVector *pv = new PigVector ( (rc * cos(out_az)), (rc * sin(out_az)), out_range) ;
    //printf ("pv = [%f, %f]\n", pv->getX(), pv->getY());
    fp = new PigPoint2D(pv->getX() + proj->_nadir_sample, proj->_nadir_line - pv->getY());

    if (fp == 0) {
	env->ExceptionDescribe();
	clazz = env->FindClass ("java/lang/InstantiationException");
	env->ThrowNew (clazz, "getFromLookVector: could not make a C++ PigPoint2D: out of memory!");
	delete surf;
	delete fp;
	delete pv;
	return 0;
    }
    //printf ("fp = [%f, %f] samp %.4f line %.4f\n", fp->getX(), fp->getY(), proj->_nadir_sample, proj->_nadir_line);
    //printf ("2 origin = %.3f, %.3f, %.3f\n", co->getX(), co->getY(), co->getZ());
    //printf ("look = %.3f, %.3f, %.3f\n", look->getX(), look->getY(), look->getZ());
    if ((makeJObject (env, obj, "jpl/mipl/mica/pig/PigPoint2D", "(DD)V", &pointClazz, &mid)) != 1) {
      delete surf;
      delete fp;
      delete pv;

      return 0;
    }

    newPoint = env->NewObject(pointClazz, mid, fp->getX(), fp->getY());
    if (newPoint == 0) {
      env->ExceptionDescribe();
      clazz = env->FindClass ("java/lang/InstantiationException");
      env->ThrowNew (clazz, "getFromLookVector: could not make a jpl.mipl.mica.pig.PigPoint2D");
      delete surf;
      delete fp;
      delete pv;
      return 0;
    }
    delete surf;
    delete fp;
    delete pv;
  
    return newPoint;
  }
  /* -=================================================================- */  
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    getHeight
   * Signature: ()D
   */
  JNIEXPORT jdouble JNICALL Java_jpl_mipl_mica_projection_PolarProjection_getHeight
  (JNIEnv *env, jobject obj)
  {
    PigPolarProjection *proj;
    
    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return 0;
 
    return proj->getHeight();
  }
  /* -=================================================================- */
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    getLookVector
   * Signature: (DDLjpl/mipl/mica/pig/CoordSystem;)Ljpl/mipl/mica/pig/PigVector;
   */
  JNIEXPORT jobject JNICALL Java_jpl_mipl_mica_projection_PolarProjection_getLookVector
  (JNIEnv *env, jobject obj, jdouble sample, jdouble line, jobject CoordSys)
  {
    PigPolarProjection *proj;
    PigCoordSystem     *cs;
    PigVector          p;
    jclass             clazz;
    jclass             pointClazz=0;
    jmethodID          mid=0;
    jobject            newPoint=0;

    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return 0;

    cs = (PigCoordSystem*)resolveField (env, CoordSys);
    if (cs == 0)
      return 0;

    p = proj->getLookVector (sample, line, cs);
    //printf ("here is getLookVector::p [ %.3f, %.3f, %.3f ]\n", p.getX(), p.getY(), p.getZ());

    if ((makeJObject (env, obj, "jpl/mipl/mica/pig/PigPoint", "(DDD)V", &pointClazz, &mid)) != 1)
      return 0;
    // got look vector and turning it into java...
    newPoint = env->NewObject(pointClazz, mid, p.getX(), p.getY(), p.getZ());
    if (newPoint == 0) {
      env->ExceptionDescribe();
      clazz = env->FindClass ("java/lang/InstantiationException");
      env->ThrowNew (clazz, "getLookVector: could not make a jpl.mipl.mica.pig.PigPoint");
      return 0;
    }

    return newPoint;

  }
  /* -=================================================================- */
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    getMaxAz
   * Signature: ()D
   */
  JNIEXPORT jdouble JNICALL Java_jpl_mipl_mica_projection_PolarProjection_getMaxAz
  (JNIEnv *env, jobject obj)
  {
    PigPolarProjection *proj;
    
    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return 0;
 
    return proj->getMaxAz();
  }
  /* -=================================================================- */

  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    getMaxEl
   * Signature: ()D
   */
  JNIEXPORT jdouble JNICALL Java_jpl_mipl_mica_projection_PolarProjection_getMaxEl
  (JNIEnv *env, jobject obj)
  {
    PigPolarProjection *proj;
    
    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return 0;
 
    return proj->getMaxEl();
  }
  /* -=================================================================- */
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    getMinAz
   * Signature: ()D
   */
  JNIEXPORT jdouble JNICALL Java_jpl_mipl_mica_projection_PolarProjection_getMinAz
  (JNIEnv *env, jobject obj)
  {
    PigPolarProjection *proj;
    
    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return 0;
 
    return proj->getMinAz();
  }
  /* -=================================================================- */
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    getMinElev
   * Signature: ()D
   */
  JNIEXPORT jdouble JNICALL Java_jpl_mipl_mica_projection_PolarProjection_getMinElev
  (JNIEnv *env, jobject obj)
  {
    PigPolarProjection *proj;
    
    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return 0;
    
    return proj->getMinEl();
  }
  /* -=================================================================- */
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    getNaturalScale
   * Signature: ()D
   */
  JNIEXPORT jdouble JNICALL Java_jpl_mipl_mica_projection_PolarProjection_getNaturalScale
  (JNIEnv *env, jobject obj)
  {
    PigPolarProjection *proj;
    
    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return 0;
    
    return proj->getNaturalScale ();
  }
  /* -=================================================================- */
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    setNaturalScale
   * Signature: (D)V
   */
  JNIEXPORT void JNICALL Java_jpl_mipl_mica_projection_PolarProjection_setNaturalScale
  (JNIEnv *env, jobject obj, jdouble newScale)
  {
    PigPolarProjection *proj;
    
    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return;
    
    return proj->setNaturalScale( newScale );
  }
  /* -=================================================================- */
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    getOrigin
   * Signature: ()Ljpl/mipl/mica/pig/PigPoint;
   */
  JNIEXPORT jobject JNICALL Java_jpl_mipl_mica_projection_PolarProjection_getOrigin
  (JNIEnv *env, jobject obj)
  {
    PigPolarProjection   *proj;
    PigVector             *p;
    jclass                clazz;
    jclass                pointClazz;
    jmethodID             mid=0;
    jobject               newPoint=0;

    //printf ("starting call to getOrigin\n");
    proj = (PigPolarProjection*)resolveField(env, obj);

    if (proj == 0)
      return 0;

    //p = proj->getOrigin ();
    //printf ("[ %.3f %.3f %.3f ]\n", p->getX(), p->getY(), p->getZ());

    if ((makeJObject (env, obj, "jpl/mipl/mica/pig/PigPoint", "(DDD)V", &pointClazz, &mid)) != 1)
      return 0;

    newPoint = env->NewObject(pointClazz, mid, proj->getOrigin()->getX(), proj->getOrigin()->getY(), proj->getOrigin()->getZ());
    if (newPoint == 0) {
      env->ExceptionDescribe();
      clazz = env->FindClass ("java/lang/InstantiationException");
      env->ThrowNew (clazz, "getOrigin: could not make a jpl.mipl.mica.pig.PigPoint");
      return 0;
    }

    return newPoint;

  }
  /* -=================================================================- */  
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    getScale
   * Signature: ()D
   */
  JNIEXPORT jdouble JNICALL Java_jpl_mipl_mica_projection_PolarProjection_getScale
  (JNIEnv *env, jobject obj)
  {
    PigPolarProjection *proj;
    
    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return 0;
    
    return proj->getScale ();
  }
  /* -=================================================================- */
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    getSurfacePoint
   * Signature: (Ljpl/mipl/mica/pig/SurfaceModel;Ljpl/mipl/mica/pig/PigVector;Ljpl/mipl/mica/pig/PigPoint;)Ljpl/mipl/mica/pig/PigPoint;
   */
  JNIEXPORT jobject JNICALL Java_jpl_mipl_mica_projection_PolarProjection_getSurfacePoint
  (JNIEnv *env, jobject obj, jobject SurfModel, jobject LookVect, jobject SurfVect)
  {
    jclass                clazz;
    jclass                pointClazz=0;
    jmethodID             mid=0;
    jobject               newPoint=0;
    PigPoint              *p;
    PigPolarProjection    *proj;
	int					infinity;

    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return 0;

    PigSurfaceModel *sm = (PigSurfaceModel*)resolveField (env, SurfModel);
    if (sm == 0)
      return 0;

    PigVector *look = (PigVector*)resolveField (env, LookVect);
    if (look == 0)
      return 0;

    PigPoint *surf = (PigPoint*)resolveField (env, SurfVect);
    if (surf == 0)
      return 0;

    p = proj->getSurfacePoint (*sm, *look, *surf, infinity);

    if ((makeJObject (env, obj, "jpl/mipl/mica/pig/PigPoint", "(DDD)V", &pointClazz, &mid)) != 1)
      return 0;
    //printf ("called getSurfacePoint\n");
    newPoint = env->NewObject(pointClazz, mid, p->getX(), p->getY(), p->getZ());
    if (newPoint == 0) {
      env->ExceptionDescribe();
      clazz = env->FindClass ("java/lang/InstantiationException");
      env->ThrowNew (clazz, "getSurfacePoint: could not make a jpl.mipl.mica.pig.PigPoint");
      return 0;
    }

    return newPoint;
  }
  /* -=================================================================- */
    /*
     * Class:     jpl_mipl_mica_projection_PolarProjection
     * Method:    getUpAzimuth
     * Signature: ()D
     */
    JNIEXPORT jdouble JNICALL Java_jpl_mipl_mica_projection_PolarProjection_getUpAzimuth
	(JNIEnv *env, jobject obj)
    {
	PigPolarProjection *proj;
	
	proj = (PigPolarProjection*)resolveField (env, obj);
	if (proj == 0)
	    return 0;
	
	return proj->getUpAzimuth();
    }
  /* -=================================================================- */    
    /*
     * Class:     jpl_mipl_mica_projection_PolarProjection
     * Method:    getWidth
     * Signature: ()D
     */
    JNIEXPORT jdouble JNICALL Java_jpl_mipl_mica_projection_PolarProjection_getWidth
	(JNIEnv *env, jobject obj)
    {
	PigPolarProjection *proj;
	
	proj = (PigPolarProjection*)resolveField (env, obj);
	if (proj == 0)
	    return 0;
	
	return proj->getWidth();
    }
  /* -=================================================================- */
    /*
     * Class:     jpl_mipl_mica_projection_PolarProjection
     * Method:    setAzFirstSample
     * Signature: (D)V
     */
    JNIEXPORT void JNICALL Java_jpl_mipl_mica_projection_PolarProjection_setAzFirstSample
	(JNIEnv *env, jobject obj, jdouble azSample)
    {
	PigPolarProjection *proj;
	
	proj = (PigPolarProjection*)resolveField (env, obj);
	if (proj == 0)
	    return ;
	
	proj->setAzFirstSample(azSample);
    }
  /* -=================================================================- */

  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    setMaxAz
   * Signature: (D)V
   */
  JNIEXPORT void JNICALL Java_jpl_mipl_mica_projection_PolarProjection_setMaxAz
  (JNIEnv *env, jobject obj, jdouble maxAz)
  {
    PigPolarProjection *proj;
    
    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return ;
    
    proj->setMaxAz (maxAz);
  }
  /* -=================================================================- */
  
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    setMaxElev
   * Signature: (D)V
   */
  JNIEXPORT void JNICALL Java_jpl_mipl_mica_projection_PolarProjection_setMaxElev
  (JNIEnv *env, jobject obj, jdouble maxEl)
  {
    PigPolarProjection *proj;
    
    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return ;
    
    proj->setMaxEl (maxEl);
  }
  /* -=================================================================- */
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    setMinAz
   * Signature: (D)V
   */
  JNIEXPORT void JNICALL Java_jpl_mipl_mica_projection_PolarProjection_setMinAz
  (JNIEnv *env, jobject obj, jdouble minAz)
  {
    PigPolarProjection *proj;
    
    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return ;
    
    proj->setMinAz (minAz);
  }
  /* -=================================================================- */
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    setMinElev
   * Signature: (D)V
   */
  JNIEXPORT void JNICALL Java_jpl_mipl_mica_projection_PolarProjection_setMinElev
  (JNIEnv *env, jobject obj, jdouble minEl)
  {
    PigPolarProjection *proj;
    
    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return ;
    
    proj->setMinEl (minEl);
  }
  /* -=================================================================- */
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    setNtvOrigin
   * Signature: (Ljpl/mipl/mica/pig/PigPoint;)V
   */
  JNIEXPORT void JNICALL Java_jpl_mipl_mica_projection_PolarProjection_setOrigin
  (JNIEnv *env, jobject obj, jdouble xx, jdouble yy, jdouble zz)
  {
    PigPolarProjection *proj;
    
    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return ;

    printf ("setting origin %f %f %f\n", xx, yy, zz);
    PigPoint *p = new PigPoint (xx, yy, zz);
    proj->setOrigin (p);
  }
  /* -=================================================================- */  
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    setScale
   * Signature: (D)V
   */
  JNIEXPORT void JNICALL Java_jpl_mipl_mica_projection_PolarProjection_setScaleNtv
  (JNIEnv *env, jobject obj, jdouble scale)
  {
    PigPolarProjection *proj;

    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return ;

    proj->setScale (scale);
  }
  /* -=================================================================- */
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    setUpAzimuth
   * Signature: (D)V
   */
  JNIEXPORT void JNICALL Java_jpl_mipl_mica_projection_PolarProjection_setUpAzimuth
  (JNIEnv *env, jobject obj, jdouble uAz)
  {
    PigPolarProjection *proj;
    
    proj = (PigPolarProjection*)resolveField (env, obj);
    if (proj == 0)
      return ;

    proj->setUpAzimuth (uAz);

  }

    /* -============================================================- */
  /*
   * Class:     PigPolarProjectionImpl
   * Method:    initialize
   * Signature: (Ljpl/mipl/mica/pig/CoordSystem;)I
   */
  JNIEXPORT jint JNICALL Java_jpl_mipl_mica_projection_PolarProjection_initialize
  (JNIEnv *env, jobject obj, jobject coors, jobject surfm, 
   			jint size, jobjectArray filem, jobjectArray cameram, 
			jobjectArray pointm )
  {
    jboolean        	isCopy;
    const char      	*fname;
    const char      	*mission;
    char		      	missionStr[64];
    char      			instrument[64];
    char	  			*filenames[400];
    PigFileModel    	*fm[400];
    PigCameraModel  	*cm[400];
    PigPointingModel	*pm[400];
    PigSurfaceModel 	*sm;
    PigCoordSystem  	*cs;
	PigPolarProjection *projection;
	int					nlo, nso;
	double 				scale;				// radians/pixel, cyl/polar only
	PigPoint 			proj_origin;		// cylindrical/polar only
	double 				up_azimuth;			// polar only
	double 				az_first_sample;	// cylindrical only
    jclass          	clazz=0;
    jfieldID        	fid=0;
    int 				i, j;
	int 				homogeneous_inputs = TRUE;
	int					azdir;				// +1 ==CW, -1 == CCW
	double	 			min_elev, max_elev;	
	double	 			min_az, max_az;	
	double	 			start_az, stop_az;	
	double	 			*az_low, *az_high;	
    int              	nids;
    int              	status;
    char             	msg[PIG_MAX_FILENAME_SIZE+1];
    char             	listoffiles[PIG_MAX_FILENAME_SIZE+1];
    FILE             	*fd;

//    printf("In libutil_native\n");

	nids = size;

	// get all of the underlying c++ objects so we can deal with them.
	// We don't get the CoordSystem and SurfaceModel because we will create
	// them later.  We will also create the camera models and pointing models.

	jobject element;
		const char * tempname;
	for ( i = 0; i < nids; i++ ) {
		element = env->GetObjectArrayElement( filem, i );
		fm[i] = (PigFileModel*)resolveField( env, element );
	
		tempname = fm[i]->getFilename();
		//printf("Filename is %s \n", tempname );
//		element = env->GetObjectArrayElement( cameram, i );
//		cm[i] = (PigCameraModel*)resolveField( env, element );
//		element = env->GetObjectArrayElement( pointm, i );
//		pm[i] = (PigPointingModel*)resolveField( env, element );

	}

	projection = (PigPolarProjection*)resolveField( env, obj );

	//  we should load the Rover state files in here, ala mars_read_rsf, but I
	//  don't know how to access them through the vicar file.  TBD

//	printf("Getting mission name\n");
	mission=fm[0]->getMissionName();

//	printf("Getting mission %s\n", mission);
	PigMission *m = PigMission::getMissionObject(mission);

    // Read all coord systems from the input files and add them to the
    // RSM database...
/*   this code has been moved to PigFileModel initialize
	printf("Getting RoverStateManager\n");

	PigRoverStateManager *rsm = m->getRoverStateManager();
	printf("have RoverStateManager\n");
    for (int i=0; i < nids; i++) {
		printf("before add coordsystem for file \n");
		rsm->addFileCoordSystems(fm[i]);
		printf("added coordsystem for file \n");
	}
	printf("Coordinate systems have been added to RSM\n");
*/
#if 0   /* Useful for debugging... !!!! */
    rsm->printSolutions();
#endif

    // we need to look at the files...

    if (nids > 0) {       // ==0 means nothing to set

//        printf("Getting PigSite\n");
//!!!! needed always now    PigSite *site = m->getSite("FIXED");    // check if we need this...
//!!!!    if (site->isRoverSiteUsed()) {      // yep

        // Okay, we gotta look at the files...
//        printf("Site is used\n");

        int indices[PIG_MAX_CS_INDEX], num_indices;
        int min_index = 2000000000; // java: Integer.MAX_VALUE.  Sigh.

        for (int i = 0; i < nids; i++) {
        num_indices = PIG_MAX_CS_INDEX;
        fm[i]->getRoverMotionCounter(indices, num_indices);
		if (num_indices > 0) {
		    if (indices[0] < min_index)
		    min_index = indices[0];
		}
        }
//        printf("Counter stuff done\n");

        if (min_index != 2000000000)    // whoops!
        m->setFixedCS();
	//!!!! PigSite::setFixedSite(mission, min_index);
//        printf("after setFixedSite\n");
//!!!!    }
    }

    // Now set up the default coordinate system

    PigFileModel *file = NULL;
    if (nids > 0)
	    file = fm[0];

//    printf("Getting CoordSystem\n");
    cs=m->getCoordSystem(file,"FIXED");
//    printf("have CoordSystem\n");

	// now we fill the java objects with the c++ objects
	jclass claz2 = env->GetObjectClass( coors );
	if ( claz2 == 0 ) {
			env->ExceptionDescribe ();
			claz2 = env->FindClass ( "java/lang/IllegalArgumentException");
			env->ThrowNew ( claz2, "no object class");
			return 0;
	}

	// then find the field id of the field "cpp" with signature jlong
    fid = env->GetFieldID (claz2, "cpp", "J");
    if (fid == 0) {
        env->ExceptionDescribe ();
        claz2 = env->FindClass ("java/lang/NoSuchFieldError");
        env->ThrowNew (claz2, "no c++ field found in input object");
        return 0;
    }
	
    env->SetLongField ( coors, fid, (long)cs);
//    printf("after set\n");

	//sm = PigSurfaceModel::create( fm[0] );
	//sm = m->createSurfaceModel( fm[0] );
	//if (sm == 0)
	 // return 0;
	int count;
	char surface[20];
	double vector[3];
	PigVector normal(0.0, 0.0, -1.0);
	PigPoint ground(0.0, 0.0, 0.294);

	//PigCoordSystem *cs = PigCoordSystem::getFixedFrame(getMissionName());

	// Check normal
	
//    printf("getting normal \n");
	// I am getting this from the Java SurfaceModel object instead of calling
	// PigSurfaceModel::create or using similar code here, because the 
	// getStaticParam call is not working correctly even the the correct 
	// method is called with getParam ( which is protected ).
	//
	claz2 = env->GetObjectClass( surfm );
	if ((getField(env, surfm, &claz2, &fid, "normal", "[D")) != 1)
	  return 0;

	jdoubleArray javaNormal = (jdoubleArray)env->GetObjectField (surfm, fid );
	jdouble* jvec = env->GetDoubleArrayElements( javaNormal, NULL );
	if ( jvec == NULL )
		return 0;
	else {
		normal.setXYZ( jvec );
		//printf("jvec = %e %e %e  \n", jvec[0], jvec[1], jvec[2]);
		//printf("normal = %e %e %e  \n", normal.getX(), normal.getY(), normal.getZ());
	}
//    PigModelBase::getStaticParam("NORMAL", vector, &count, 3, 0);
//    m->getStaticParam("NORMAL", vector, &count, 3, 0);
//		printf("count = %d  \n", count);
	//if (count == 3) {
		//normal.setXYZ(vector);
		//printf("got 3  \n");
	//}
	
	// Check ground

 //   printf("getting ground \n");
	if ((getField(env, surfm, &claz2, &fid, "ground", "[D")) != 1)
	  return 0;

	jdoubleArray javaGround = (jdoubleArray)env->GetObjectField (surfm, fid );
	jvec = env->GetDoubleArrayElements( javaGround, NULL );
	if ( jvec == NULL )
		return 0;
	else {
		ground.setXYZ( jvec );
//		printf("jvec = %e %e %e  \n", jvec[0], jvec[1], jvec[2]);
//		printf("ground = %e %e %e  \n", ground.getX(), ground.getY(), ground.getZ());
	}
//	printf("getting ground \n");
//	PigModelBase::getStaticParam((char *)"GROUND", (void *)vector, 
//									&count, 3, 0);
//		printf("count = %d  \n", count);
//	PigMICAParams::PigMICAParamGetter((char*)"GROUND", (void *)vector, 
//								&count, 3, 0, NULL );
//	if (count == 3)
//		ground.setXYZ(vector);

	// Check surface (infinity means the above is ignored)

	if ((getField(env, surfm, &claz2, &fid, "surface", 
							"Ljava/lang/String;")) != 1)
		return 0;
		
	jstring jSurface = (jstring)(env)->GetObjectField( surfm, fid );
	const char* mySurface = env->GetStringUTFChars (jSurface, NULL);
//	printf( "mySurface is %s \n", mySurface );	
//	PigModelBase::getStaticParam("SURFACE", surface, &count, 1, 0);
//	if (strcasecmp(surface, "INFINITY") == 0)
	if (strcasecmp(mySurface, "INFINITY") == 0) {
		//printf( "Creating PigSurfaceInfinity \n" );
		sm = (PigSurfaceModel *) new PigSurfaceInfinity(NULL, cs);
	} else {
		// must be plane...
		//printf( "Creating PigSurfacePlane \n" );
		sm = (PigSurfaceModel *) new PigSurfacePlane(normal, ground, cs);
	}

	(env)->ReleaseStringUTFChars( jSurface, mySurface );

	sm->setCoordSystem( cs );

	claz2 = env->GetObjectClass( surfm );
	if ((getField(env, surfm, &claz2, &fid, "cpp", "J")) != 1)
	  return 0;

	env->SetLongField (surfm, fid, (long)sm);

	// We need to get the class and field information set up for the
	// camera and pointing models so that after we create them we can 
	// stuff the pointers to them in the Java objects

	jclass cmClass = env->GetObjectClass( 
							env->GetObjectArrayElement( cameram, 0 ) );
	if ( cmClass == 0 ) {
			env->ExceptionDescribe ();
			cmClass = env->FindClass ( "java/lang/IllegalArgumentException");
			env->ThrowNew ( cmClass, "no object class for camera model");
			return 0;
	}

	// then find the field id of the field "cpp" with signature jlong
    jfieldID cmFid = env->GetFieldID (cmClass, "cpp", "J");
    if (cmFid == 0) {
        env->ExceptionDescribe ();
        cmClass = env->FindClass ("java/lang/NoSuchFieldError");
        env->ThrowNew (cmClass, "no c++ field found in input object");
        return 0;
    }
	
	jclass pmClass = env->GetObjectClass( 
							env->GetObjectArrayElement( pointm, 0 ) );
	if ( pmClass == 0 ) {
		env->ExceptionDescribe ();
		pmClass = env->FindClass ( "java/lang/IllegalArgumentException");
		env->ThrowNew ( pmClass, "no object class for camera model");
		return 0;
	}

	// then find the field id of the field "cpp" with signature jlong
    jfieldID pmFid = env->GetFieldID (pmClass, "cpp", "J");
    if (pmFid == 0) {
        env->ExceptionDescribe ();
        pmClass = env->FindClass ("java/lang/NoSuchFieldError");
        env->ThrowNew (pmClass, "no c++ field found in input object");
        return 0;
    }
	
	// now we loop through and compute the initial camera and pointing models
		const char * name;
	
	for ( i = 0; i < nids; i++ ) {
		name = fm[i]->getFilename();
		//printf("Filename is %s \n", name );
		cm[i] = PigCameraModel::create(name, NULL );
//		cm[i] = PigCameraModel::create(fm[i], NULL );
		if ( cm[i] == NULL )
				printf( "Unable to create camera model for input %d \n", i );
			    
		pm[i] = PigPointingModel::create(cm[i], fm[i], NULL, true);

		if (pm[i] == NULL) 
			printf( "Unable to create pointing model for input %d", i);

	// Check for varying missions and instruments, for information only
	     if (i != 0) {
			 if (strcasecmp(missionStr, cm[i]->getMissionName()) != 0) {
				 printf( "Note: Input list contains more than one mission: %s", missionStr);
				 homogeneous_inputs = FALSE;
			 }
			 if (strcasecmp(instrument, cm[i]->getInstrumentName()) != 0) {
				 printf( "Note: Input list contains more than one instrument: %s" , instrument);
				 homogeneous_inputs = FALSE;
			 }
		 }
		 strcpy(missionStr, cm[i]->getMissionName());
		 strcpy(instrument, cm[i]->getInstrumentName());
																						 // Point the input camera
		 pm[i]->pointCamera(fm[i]);

		 // Stuff the Java objects with the c++ pointers
		element = env->GetObjectArrayElement( cameram, i );
		env->SetLongField ( element, cmFid, (long)cm[i] );
		element = env->GetObjectArrayElement( pointm, i );
		env->SetLongField ( element, pmFid, (long)pm[i] );
	}

	azdir = cs->getAzimuthDirection();
	//printf("Mosaic is projected in the %s coordinate frame",
	//			        cs->getFrameName());

	// Determine the min and max azimuth and elevation.  This is only
	// approximate, but that's okay since it is only used to determine
	// the output size.  This code is taken from mars_get_azel_minmax.cc
	
	min_elev = 1e8;             // these are in radians
	max_elev = -1e8;

	az_low = new double[nids];
	az_high = new double[nids];

	if (az_low == NULL || az_high == NULL) {
		snprintf(msg,PIG_MAX_FILENAME_SIZE+1,"Fatal memory error in mars_get_azel_minmax!", "");
		return 0;
	}

	// If the azimuth is more than 180 degrees away from the "center", then
	// we assume it must have wrapped and make the az <0 or >360 (to retain
	// the image limits on the proper sides of the center).
#define MINMAX_AZEL_CHECK                       \
    azim = cs->getAz(look);                     \
    if (azim >= PigDeg2Rad(360.0)) azim -= PigDeg2Rad(360.0);   \
    if (azim < 0) azim += PigDeg2Rad(360.0);            \
    if (azim - az_center > PigDeg2Rad(180.0))           \
        azim -= PigDeg2Rad(360.0);                  \
    else if (az_center - azim > PigDeg2Rad(180.0))          \
        azim += PigDeg2Rad(360.0);                  \
    elev = cs->getEl(look);                     \
    if (azim > az_high[i]) az_high[i] = azim;           \
    if (azim < az_low[i]) az_low[i] = azim;             \
	if (elev > max_elev) max_elev = elev;               \
	if (elev < min_elev) min_elev = elev;

	for (i=0; i < nids; i++) {
		PigPoint origin;
		PigVector look;
		double line, samp;
		double azim, elev;
		double sl, ss, el, es;
		double az_center;

		look = pm[i]->getCameraOrientation(cs);
		az_center = cs->getAz(look);        // in order to check for wrap
		if (az_center >= PigDeg2Rad(360.0)) az_center -= PigDeg2Rad(360.0);
		if (az_center < 0) az_center += PigDeg2Rad(360.0);
		az_low[i] = az_high[i] = az_center; // just in case...

		fm[i]->getImageBorders(sl, ss, el, es);

		cm[i]->LStoLookVector(sl, ss, origin, look, cs); // UL corner
		MINMAX_AZEL_CHECK

		cm[i]->LStoLookVector(sl, es, origin, look, cs); // UR corner
		MINMAX_AZEL_CHECK

		cm[i]->LStoLookVector(el, es, origin, look, cs); // LR corner
		MINMAX_AZEL_CHECK

		cm[i]->LStoLookVector(el, ss, origin, look, cs); // LL corner
		MINMAX_AZEL_CHECK

		cm[i]->LStoLookVector((sl+el)/2, ss, origin, look, cs);  // left
		MINMAX_AZEL_CHECK

		cm[i]->LStoLookVector((sl+el)/2, es, origin, look, cs);  // right
		MINMAX_AZEL_CHECK

		cm[i]->LStoLookVector(sl, (ss+es)/2, origin, look, cs);  // top
		MINMAX_AZEL_CHECK

		cm[i]->LStoLookVector(el, (ss+es)/2, origin, look, cs);  // btm
		MINMAX_AZEL_CHECK

		// Now check for wrap and adjust accordingly (make az_low > az_high
		// if wrap occurred)
		if (az_low[i] < 0.0 && az_high[i] >= PigDeg2Rad(360.0)) {
			az_low[i] = 0.0;                // both wrapped??
			az_high[i] = PigDeg2Rad(360.0);     // just use max range
		}
		else if (az_low[i] < 0.0)           // low side wrapped
			az_low[i] += PigDeg2Rad(360.0);
		else if (az_high[i] >= PigDeg2Rad(360.0))   // high side wrapped
			az_high[i] -= PigDeg2Rad(360.0);
	}
		
	// Determines the azimuth range for a set of images, where each is marked
	// by az_low[i] and az_high[i].  This is complicated by the possibility
	// of wrapping around (az=0/360); we want to find the smallest contiguous
	// range that includes all the images.  The min_az and max_az are returned.
	// If min_az > max_az, then the image wrapped around, i.e. the 0 point is
	// in the middle.  All values are in radians.
	//
	// The algorithm is from Jean Lorre.
	// The code is from mars_get_az_range.cc
	////////////////////////////////////////////////////////////////////////
	//
	int flags[360];     // one per degree
	double azl[360], azh[360];  // min and max edge for that degree
	
	for (i=0; i < 360; i++) {
		flags[i] = 0;
		azl[i] = 1e8;
		azh[i] = -1e8;
	}
	
	// Fill in the flags for each degree column covered by each image
	
	for (i=0; i < nids; i++) {
		double az1 = PigRad2Deg(az_low[i]);
		double az2 = PigRad2Deg(az_high[i]);
		if (az1 >= 360.) az1 -= 360.;
		if (az1 < 0.) az1 += 360.;
		if (az2 >= 360.) az2 -= 360.;
		if (az2 < 0.) az2 += 360.;
	
		// Fill in the easy way
	
		if (az2 > az1) {
			for (j = (int)az1; j <= (int)az2; j++)
				flags[j] = 1;
		}
		else {
	
			// Fill in 2 pieces... az1..360 and 0..az2
	
			for (j = (int)az1; j < 360; j++)
				flags[j] = 1;
			for (j = 0; j <= (int)az2; j++)
				flags[j] = 1;
	
			// And make sure the edges cover everything
	
			azl[0] = 0.0;
			azh[359] = 360.0;
		}
	
		// Fill in actual values of edges
		
		if (azl[(int)az1] > az1) azl[(int)az1] = az1;
		if (azh[(int)az2] < az2) azh[(int)az2] = az2;
	}
		
	// Now determine the azimuth limits by examining the flags
		
	if (flags[0] && flags[359]) {       // bridge the gap
		j = 0;
		for (i=1; i < 360; i++) {
			if (!flags[i]) {            // found the first empty column
				max_az = azh[i-1];      // it's the max
				j = i;
				break;
			}
		}
		if (j == 0) {               // Entire thing is full
			min_az = 0.0;
			max_az = 360.0;
		}
		else {
			for (i=j; i < 360; i++) {
				if (flags[i]) {         // found the first full column
					min_az = azl[i];        // it's the min
					break;
				}
			}
		}
	}
	else {                  // No bridging
		for (i=0; i < 360; i++) {
			if (flags[i]) {         // found the first full column
				min_az = azl[i];        // it's the min
				break;
			}
		}
		for (i=359; i > 0; i--) {
			if (flags[i]) {         // found the last full column
				max_az = azh[i];        // it's the max
				break;
			}
		}
	}

	// Renormalize to radians
	
	max_az = PigDeg2Rad(max_az);
	min_az = PigDeg2Rad(min_az);
	
	delete[] az_low;
	delete[] az_high;
						
	//printf("Elevation minimum %f, Elevation maximum %f\n",
	 //                      PigRad2Deg(min_elev), PigRad2Deg(max_elev));
	//printf("Azimuth minimum %f, Azimuth maximum %f\n",
	 //                      PigRad2Deg(min_az), PigRad2Deg(max_az));
	
	if (azdir == 1) {
		start_az = min_az;
		stop_az = max_az;
	}
	else {
        start_az = max_az;
		stop_az = min_az;
	}

	// Determine location of the center of projection.  This defaults
	// to the average of all the input camera locations, but may be
	// overridden.
	
	proj_origin.setXYZ(0.0, 0.0, 0.0);
	for (i=0; i < nids; i++) {
		proj_origin += pm[i]->getCameraPosition(cs);
	}
	proj_origin /= (double)nids;
	
	//printf("Projection origin = (%f, %f, %f)\n",
	//		proj_origin.getX(), proj_origin.getY(), proj_origin.getZ());

	// Determine the scale of the output by looking at the first input's
	// horizontal direction (the direction is arbitrary).
	
	scale = cm[0]->getPixelAngle(0);     // radians per pixel
	printf("Pixel scale: %f radians/pixel or %f pixels/degree\n",
	                     scale, 1.0/PigRad2Deg(scale));
	
	// Start at nadir always, and it's circular
	nlo = (int)(2.0 * (max_elev - PigDeg2Rad(-90.)) /scale);
	if ( nlo % 2 == 0)
			nlo++;		// make sure it's odd so we have a center pixel
	nso = nlo;			// always square

//	nadir_line = nlo /2;		// rounds down
//	nadir_samp = nso / 2;

	// Get the azimuth to put up (normally 0) in the output.
	// Defaults (in the PDF) to 0.0.  Other values have the effect
	// of rotating the entire image so the given value is at the 
	// top of the mosaic.
	//
	up_azimuth = PigDeg2Rad( 0.0 );


	//printf( "line of zero elevation = %f\n", line_zero_el);

	// Hopefully at this point we have all the information necessary to do
	// a good job on the projection.  Now we set this information in the
	// projection object and cross our fingers.
	
	//projection->setLineZeroElevation( line_zero_el );
	//projection->setOrigin( &proj_origin );
	projection->setOrigin( new PigPoint( proj_origin.getX(), 
						proj_origin.getY(), proj_origin.getZ()));
	projection->setMinAz( min_az );
	projection->setMaxAz( max_az );
	projection->setMinEl( min_elev );
	projection->setMaxEl( max_elev );
	projection->setNaturalScale( scale );
	// Arbitrarily reduce scale so that images fit screen better
	projection->setScale( scale*4 );
	projection->setUpAzimuth( up_azimuth );
	projection->setAzFirstSample( start_az );
//	projection->setAzFirstSample( az_first_sample );

    return 1;

  }
  
  /*
   * Class:     jpl_mipl_mica_projection_PolarProjection
   * Method:    setRMCDB
   * Signature: (Ljava/lang/String;[Ljava/lang/String;)V
   */
  JNIEXPORT void JNICALL Java_jpl_mipl_mica_projection_PolarProjection_setRMCDB
	    (JNIEnv *env, jobject obj, jstring file, jobjectArray rmcFiles, jint nids)
	{
		const char * fname;
		const char      	*mission;
		const char * rmcname;
		jboolean isCopy;
		PigFileModel* fm;

		fname = env->GetStringUTFChars( file, &isCopy );

		fm = PigFileModel::create( fname );

		env->ReleaseStringUTFChars (file, fname);

		mission=fm->getMissionName();
		PigMission *m = PigMission::getMissionObject(mission);
		PigRoverStateManager *rsm = m->getRoverStateManager();
		// Now go through the path and call readFile on each
		
		jstring element;
		for ( int i = 0; i < nids; i++ ) {
			element = (jstring)env->GetObjectArrayElement( rmcFiles, i );
			rmcname = env->GetStringUTFChars( element, &isCopy );
			rsm->readSolutionFile( (char *)rmcname, true);
			env->ReleaseStringUTFChars (element, rmcname);
		}
		

	}
} 



