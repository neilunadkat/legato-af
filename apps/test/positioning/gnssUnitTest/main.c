/**
 * This module implements the unit tests for GNSS API.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include "pa_gnss.h"
#include "pa_gnss_simu.h"
#include "le_gnss_local.h"
#include "le_log.h"

//--------------------------------------------------------------------------------------------------
/**
 * SV ID definitions corresponding to SBAS constellation categories
 *
 */
//--------------------------------------------------------------------------------------------------
// EGNOS SBAS category
#define SBAS_EGNOS_SV_ID_33            33
// WAAS SBAS category
#define SBAS_WAAS_SV_ID_51             51
// GAGAN SBAS category
#define SBAS_GAGAN_SV_ID_40            40
// MSAS SBAS category
#define SBAS_MSAS_SV_ID_50             50
// Unknown category
#define UNKNOWN                         0

//--------------------------------------------------------------------------------------------------
/**
 * Set the SUPL certificate id
 *
 */
//--------------------------------------------------------------------------------------------------
#define SUPL_CERTIFICATE_ID          0x69

//--------------------------------------------------------------------------------------------------
/**
 * Maintain the certificate.
 */
//--------------------------------------------------------------------------------------------------
static char ShortSuplCertificate[50] = {0};

//--------------------------------------------------------------------------------------------------
/**
 * Mutex used to protect access to le_gnss functions when used in different threads.
 */
//--------------------------------------------------------------------------------------------------
static pthread_mutex_t Mutex = PTHREAD_MUTEX_INITIALIZER;   // POSIX "Fast" mutex.
/// Locks the mutex.
#define LOCK    LE_ASSERT(pthread_mutex_lock(&Mutex) == 0);
/// Unlocks the mutex.
#define UNLOCK  LE_ASSERT(pthread_mutex_unlock(&Mutex) == 0);

//--------------------------------------------------------------------------------------------------
/**
 * Maintains the dummy sample position handler reference.
 */
//--------------------------------------------------------------------------------------------------
static le_gnss_SampleRef_t GnssPositionSampleRef;

//--------------------------------------------------------------------------------------------------
/**
 * PA handler's reference.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_gnss_PositionHandlerRef_t GnssPositionHandlerRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Thread and semaphore reference.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_sem_Ref_t                 ThreadSemaphore;
static le_thread_Ref_t              AppThreadRef;
static le_clk_Time_t                TimeToWait = { 5, 0 };

//--------------------------------------------------------------------------------------------------
/**
 * Handler function for Position Notifications.
 *
 */
//--------------------------------------------------------------------------------------------------
static void GnssPositionHandlerFunction
(
    le_gnss_SampleRef_t positionSampleRef,
    void* contextPtr
)
{
    le_result_t result;
    // Date parameters
    uint16_t year;
    uint16_t month;
    uint16_t day;
    int32_t altitude;
    int32_t vAccuracy;
    // Time parameters
    uint16_t hours;
    uint16_t minutes;
    uint16_t seconds;
    uint16_t milliseconds;
    // GPS time
    uint32_t gpsWeek;
    uint32_t gpsTimeOfWeek;
    // Leap seconds in advance
    uint8_t leapSeconds;
    // Position state
    le_gnss_FixState_t state;
    // Location
    int32_t latitude;
    int32_t longitude;
    int32_t altitudeOnWgs84;
    int32_t hAccuracy;
    int32_t magneticDeviation;
    // DOP parameter
    uint16_t dop;
    // Horizontal speed
    uint32_t hSpeed;
    uint32_t hSpeedAccuracy;
    // Vertical speed
    int32_t vSpeed;
    int32_t vSpeedAccuracy;
    // Direction
    uint32_t direction;
    uint32_t directionAccuracy;
    le_gnss_DopType_t dopType = LE_GNSS_PDOP;
    uint64_t EpochTime;
    uint16_t hdopPtr, vdopPtr, pdopPtr;
    uint32_t timeAccuracy;

    LE_ASSERT(positionSampleRef != NULL);

    // Get UTC date
    result = le_gnss_GetDate(positionSampleRef, NULL, NULL, NULL);
    LE_ASSERT(LE_FAULT == result);
    result = le_gnss_GetDate(positionSampleRef, &year, &month, &day);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));

    // Get altitude
    result = le_gnss_GetAltitude(positionSampleRef, &altitude, &vAccuracy);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    // Pass invalid sample reference
    result = le_gnss_GetAltitude(GnssPositionSampleRef, &altitude, &vAccuracy);
    LE_ASSERT(LE_FAULT == result);

    // Get UTC time
    result = le_gnss_GetTime(positionSampleRef, &hours, &minutes, &seconds, &milliseconds);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    result = le_gnss_GetTime(positionSampleRef, NULL, NULL, NULL, NULL);
    LE_ASSERT(LE_FAULT == result);
    // Pass invalid sample reference
    result = le_gnss_GetTime(GnssPositionSampleRef, &hours, &minutes, &seconds, &milliseconds);
    LE_ASSERT(LE_FAULT == result);

    // Get Epoch time
    LE_ASSERT(LE_FAULT == (le_gnss_GetEpochTime(positionSampleRef, NULL)));
    result = le_gnss_GetEpochTime(positionSampleRef, &EpochTime);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    // Pass invalid sample reference
    LE_ASSERT(LE_FAULT == (le_gnss_GetEpochTime(GnssPositionSampleRef, &EpochTime)));

    // Get GPS time
    result = le_gnss_GetGpsTime(positionSampleRef, &gpsWeek, &gpsTimeOfWeek);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    LE_ASSERT(LE_FAULT == (le_gnss_GetGpsTime(positionSampleRef, NULL, NULL)));
    // Pass invalid sample reference
    LE_ASSERT(LE_FAULT == (le_gnss_GetGpsTime(GnssPositionSampleRef, &gpsWeek, &gpsTimeOfWeek)));

    // Get TimeAccurecy
    result = le_gnss_GetTimeAccuracy(positionSampleRef, &timeAccuracy);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    LE_ASSERT(LE_FAULT == (le_gnss_GetTimeAccuracy(positionSampleRef, NULL)));

    // Get UTC leap seconds in advance
    result = le_gnss_GetGpsLeapSeconds(positionSampleRef, &leapSeconds);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    LE_ASSERT(LE_FAULT == (le_gnss_GetGpsLeapSeconds(positionSampleRef, NULL)));
    // Pass invalid sample reference
    LE_ASSERT(LE_FAULT == (le_gnss_GetGpsLeapSeconds(GnssPositionSampleRef, &leapSeconds)));

    // Get position state
    LE_ASSERT(LE_FAULT == (le_gnss_GetPositionState(positionSampleRef, NULL)));
    LE_ASSERT_OK(le_gnss_GetPositionState(positionSampleRef, &state));
    // Pass invalid sample reference
    LE_ASSERT(LE_FAULT == (le_gnss_GetPositionState(GnssPositionSampleRef, &state)));

    // Get Location
    result = le_gnss_GetLocation(positionSampleRef, &latitude, &longitude, &hAccuracy);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    // Pass invalid sample reference
    LE_ASSERT(LE_FAULT == (le_gnss_GetLocation(GnssPositionSampleRef, &latitude,
                                               &longitude, &hAccuracy)));

    // Get altitude
    result = le_gnss_GetAltitudeOnWgs84(positionSampleRef, &altitudeOnWgs84);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    LE_ASSERT(LE_FAULT == (le_gnss_GetAltitudeOnWgs84(positionSampleRef, NULL)));
    // Pass invalid sample reference
    LE_ASSERT(LE_FAULT == (le_gnss_GetAltitudeOnWgs84(GnssPositionSampleRef, &altitudeOnWgs84)));

    // Get horizontal speed
    result = le_gnss_GetHorizontalSpeed(positionSampleRef, &hSpeed, &hSpeedAccuracy);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    // Pass invalid sample reference
    LE_ASSERT(LE_FAULT == (le_gnss_GetHorizontalSpeed(GnssPositionSampleRef,
                                                      &hSpeed, &hSpeedAccuracy)));

    // Get vertical speed
    result = le_gnss_GetVerticalSpeed(positionSampleRef, &vSpeed, &vSpeedAccuracy);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    // Pass invalid sample reference
    LE_ASSERT(LE_FAULT == (le_gnss_GetVerticalSpeed(GnssPositionSampleRef,
                                                    &vSpeed, &vSpeedAccuracy)));

    // Get direction
    result = le_gnss_GetDirection(positionSampleRef, &direction, &directionAccuracy);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    // Pass invalid sample reference
    LE_ASSERT(LE_FAULT == (le_gnss_GetDirection(GnssPositionSampleRef, &direction,
                                                &directionAccuracy)));

    // Get the magnetic deviation
    result = le_gnss_GetMagneticDeviation(positionSampleRef, &magneticDeviation);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    // Pass invalid sample reference
    LE_ASSERT(LE_FAULT == (le_gnss_GetMagneticDeviation(GnssPositionSampleRef,
                                                        &magneticDeviation)));

    // Get the DOP parameters
    result = le_gnss_GetDop(positionSampleRef, &hdopPtr, &vdopPtr, &pdopPtr);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    LE_ASSERT(LE_FAULT == (le_gnss_GetDop(GnssPositionSampleRef, &hdopPtr, &vdopPtr, &pdopPtr)));

    // Satellites status
    uint8_t satsInViewCount;
    uint8_t satsTrackingCount;
    uint8_t satsUsedCount;
    result =  le_gnss_GetSatellitesStatus(positionSampleRef,
                                          &satsInViewCount,
                                          &satsTrackingCount,
                                          &satsUsedCount);

    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    // Pass invalid sample reference
    LE_ASSERT(LE_FAULT == (le_gnss_GetSatellitesStatus(GnssPositionSampleRef, &satsInViewCount,
                                                       &satsTrackingCount, &satsUsedCount)));

    // Get DOP parameter
    result = le_gnss_GetDilutionOfPrecision(positionSampleRef, dopType, &dop);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    dopType = LE_GNSS_HDOP;
    result = le_gnss_GetDilutionOfPrecision(positionSampleRef, dopType, &dop);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    dopType = LE_GNSS_VDOP;
    result = le_gnss_GetDilutionOfPrecision(positionSampleRef, dopType, &dop);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    dopType = LE_GNSS_GDOP;
    result = le_gnss_GetDilutionOfPrecision(positionSampleRef, dopType, &dop);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));
    dopType = LE_GNSS_TDOP;
    result = le_gnss_GetDilutionOfPrecision(positionSampleRef, dopType, &dop);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));

    // Pass invalid sample reference
    LE_ASSERT(LE_FAULT == (le_gnss_GetDilutionOfPrecision(GnssPositionSampleRef, dopType,
                                                          &dop)));

    // Satellites information
    uint16_t satIdPtr[0];
    size_t satIdNumElements = sizeof(satIdPtr);
    le_gnss_Constellation_t satConstPtr[0];
    size_t satConstNumElements = sizeof(satConstPtr);
    bool satUsedPtr[0];
    size_t satUsedNumElements = sizeof(satUsedPtr);
    uint8_t satSnrPtr[0];
    size_t satSnrNumElements = sizeof(satSnrPtr);
    uint16_t satAzimPtr[0];
    size_t satAzimNumElements = sizeof(satAzimPtr);
    uint8_t satElevPtr[0];
    size_t satElevNumElements = sizeof(satElevPtr);

    result =  le_gnss_GetSatellitesInfo(positionSampleRef,
                                        satIdPtr,
                                        &satIdNumElements,
                                        satConstPtr,
                                        &satConstNumElements,
                                        satUsedPtr,
                                        &satUsedNumElements,
                                        satSnrPtr,
                                        &satSnrNumElements,
                                        satAzimPtr,
                                        &satAzimNumElements,
                                        satElevPtr,
                                        &satElevNumElements);
    LE_ASSERT((LE_OK == result)||(LE_OUT_OF_RANGE == result));

    le_gnss_ReleaseSampleRef(positionSampleRef);
    le_sem_Post(ThreadSemaphore);
}

//--------------------------------------------------------------------------------------------------
/**
 * Synchronize test thread (i.e. main) and tasks
 *
 */
//--------------------------------------------------------------------------------------------------
static void SynchTest
(
    void
)
{
    LE_ASSERT_OK(le_sem_WaitWithTimeOut(ThreadSemaphore, TimeToWait));
}

//--------------------------------------------------------------------------------------------------
/**
 * Test tasks: this function handles the task and run an eventLoop
 *
 */
//--------------------------------------------------------------------------------------------------
static void* AppHandler
(
    void* ctxPtr
)
{
    // Gnss pa init
    pa_gnss_Init();
    // init the services
    gnss_Init();

    LOCK
    // Subscribe position handler
    LE_ASSERT((GnssPositionHandlerRef = le_gnss_AddPositionHandler(GnssPositionHandlerFunction,
                                                                 NULL)) != NULL);
    UNLOCK
    // Semaphore is used to synchronize the task execution with the core test
    le_sem_Post(ThreadSemaphore);
    le_event_RunLoop();
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: Position handler function initialize and test
 *
 */
//--------------------------------------------------------------------------------------------------
static void Testle_gnss_AddHandlers
(
    void
)
{
    // Create a semaphore to coordinate the test
    ThreadSemaphore = le_sem_Create("HandlerSem",0);
    AppThreadRef = le_thread_Create("PositionHandlerThread", AppHandler, NULL);
    le_thread_Start(AppThreadRef);
    // Wait that the tasks have started before continuing the test
    SynchTest();
    pa_gnssSimu_ReportEvent();
    // The tasks have subscribe to event event handler:
    // wait the handlers' calls
    SynchTest();
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: Initialize the position data with valid values and trigger the position handler event.
 *
 */
//--------------------------------------------------------------------------------------------------
static void Testset_gnss_PositionData
(
    void
)
{
    LOCK
    pa_gnssSimu_SetGnssValidPositionData();
    UNLOCK
    pa_gnssSimu_ReportEvent();
    SynchTest();
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: this function handles the remove position handler
 *
 */
//--------------------------------------------------------------------------------------------------
static void RemoveHandler
(
    void* param1Ptr,
    void* param2Ptr
)
{
    le_gnss_RemovePositionHandler(GnssPositionHandlerRef);
    GnssPositionHandlerRef = NULL;
    // Semaphore is used to synchronize the task execution with the core test
    le_sem_Post(ThreadSemaphore);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test remove handlers
 *
 * API tested:
 * - le_gnss_RemoveHandlers
 *
 */
//--------------------------------------------------------------------------------------------------
static void Testle_gnss_RemoveHandlers
(
    void
)
{
    le_event_QueueFunctionToThread(AppThreadRef, RemoveHandler, NULL, NULL);
    SynchTest();
    // Provoke event to make sure handler not called anymore
    pa_gnssSimu_ReportEvent();
    // No semaphore post is waiting, we are expecting a timeout
    LE_ASSERT(le_sem_WaitWithTimeOut(ThreadSemaphore, TimeToWait) == LE_TIMEOUT);

    le_thread_Cancel(AppThreadRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Tested API: le_gnss_GetState()
 *
 * Return:
 *  - Status of GNSS device.
 */
//--------------------------------------------------------------------------------------------------
static void Testle_gnss_GetState
(
    void
)
{
    LE_ASSERT(LE_GNSS_STATE_READY == (le_gnss_GetState()));
}

//--------------------------------------------------------------------------------------------------
/**
 * Tested API: le_gnss_GetLastSampleRef()
 *
 * Return:
 *  - A reference to last Position's sample.
 */
//--------------------------------------------------------------------------------------------------
static void Testle_gnss_GetLastSampleRef
(
   void
)
{
    le_gnss_SampleRef_t mypositionSampleRef = le_gnss_GetLastSampleRef();
    LE_ASSERT(mypositionSampleRef != NULL);
}

//--------------------------------------------------------------------------------------------------
/**
 * Tested API: le_gnss_GetSbasConstellationCategory()
 *
 * Verify that le_gnss_GetSbasConstellationCategory() behaves as expected in failure and success
 */
//--------------------------------------------------------------------------------------------------
static void Testle_gnss_GetSbasConstellationCategory
(
    void
)
{
    LE_ASSERT(LE_GNSS_SBAS_EGNOS == (le_gnss_GetSbasConstellationCategory(SBAS_EGNOS_SV_ID_33)));
    LE_ASSERT(LE_GNSS_SBAS_WAAS == (le_gnss_GetSbasConstellationCategory(SBAS_WAAS_SV_ID_51)));
    LE_ASSERT(LE_GNSS_SBAS_GAGAN == (le_gnss_GetSbasConstellationCategory(SBAS_GAGAN_SV_ID_40)));
    LE_ASSERT(LE_GNSS_SBAS_MSAS == (le_gnss_GetSbasConstellationCategory(SBAS_MSAS_SV_ID_50)));
    LE_ASSERT(LE_GNSS_SBAS_UNKNOWN == (le_gnss_GetSbasConstellationCategory(UNKNOWN)));
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: test SUPL certificate
 *
 */
//--------------------------------------------------------------------------------------------------
static void Testle_gnss_SuplCertificate
(
    void
)
{
    le_gnss_AssistedMode_t gnssMode;

    memset(&ShortSuplCertificate, SUPL_CERTIFICATE_ID, sizeof(ShortSuplCertificate));

    //Gets the SUPL Assisted-GNSS LE_GNSS_STANDALONE_MODE mode.
    LE_ASSERT(LE_FAULT == (le_gnss_GetSuplAssistedMode(NULL)));
    LE_ASSERT_OK(le_gnss_GetSuplAssistedMode(&gnssMode));
    LE_INFO("Supl Assisted Mode obtained: %d",gnssMode);
    //Set the SUPL server URL
    LE_ASSERT_OK(le_gnss_SetSuplServerUrl("http://sls1.sirf"));

    //Injects the SUPL certificate with ID error
    LE_ASSERT(LE_BAD_PARAMETER == (le_gnss_InjectSuplCertificate(10,
                               strlen(ShortSuplCertificate),ShortSuplCertificate)));

    //Injects the SUPL certificate to be used in A-GNSS sessions
    LE_ASSERT_OK(le_gnss_InjectSuplCertificate(0,
                               strlen(ShortSuplCertificate),ShortSuplCertificate));

    //Delete the SUPL certificate 10 (out of range)
    LE_ASSERT(LE_BAD_PARAMETER == (le_gnss_DeleteSuplCertificate(10)));
    //Delete the SUPL certificate used in A-GNSS sessions
    LE_ASSERT_OK(le_gnss_DeleteSuplCertificate(0));
}

//--------------------------------------------------------------------------------------------------
/**
 * Tested API: le_gnss_EnableExtendedEphemerisFile(), le_gnss_DisableExtendedEphemerisFile()
 *             le_gnss_LoadExtendedEphemerisFile()
 *
 * Verify that behaves as expected
 */
//--------------------------------------------------------------------------------------------------
static void Testle_gnss_EnableDisableLoadExtendedEphemerisFile
(
    void
)
{
    int32_t fd = 0;
    LE_ASSERT(LE_FAULT == (le_gnss_EnableExtendedEphemerisFile()));
    LE_ASSERT(LE_FAULT == (le_gnss_DisableExtendedEphemerisFile()));
    LE_ASSERT(LE_FAULT == (le_gnss_LoadExtendedEphemerisFile(fd)));
}

//--------------------------------------------------------------------------------------------------
/**
 * Tested API: le_gnss_GetExtendedEphemerisValidity()
 *
 * Verify that behaves as expected
 */
//--------------------------------------------------------------------------------------------------
static void Testle_gnss_GetExtendedEphemerisValidity
(
    void
)
{
    uint64_t startTimePtr;
    uint64_t stopTimePtr;
    LE_ASSERT(LE_FAULT == (le_gnss_GetExtendedEphemerisValidity(&startTimePtr, &stopTimePtr)));
    LE_ASSERT(LE_FAULT == (le_gnss_GetExtendedEphemerisValidity(NULL, NULL)));
}

//--------------------------------------------------------------------------------------------------
/**
 * Tested API: le_gnss_InjectUtcTime()
 *
 * Verify that behaves as expected
 */
//--------------------------------------------------------------------------------------------------
static void Testle_gnss_InjectUtcTime
(
    void
)
{
    uint64_t timeUtc = 1970;
    uint32_t timeUnc = 10000;
    LE_ASSERT(LE_FAULT == (le_gnss_InjectUtcTime(timeUtc, timeUnc)));
}

//--------------------------------------------------------------------------------------------------
/**
 * Tested API: le_gnss_SetSuplAssistedMode()
 *
 * Verify that behaves as expected
 */
//--------------------------------------------------------------------------------------------------
static void Testle_gnss_SetSuplAssistedMode
(
    void
)
{
    LE_ASSERT(LE_FAULT == (le_gnss_SetSuplAssistedMode(LE_GNSS_STANDALONE_MODE)));
}

//--------------------------------------------------------------------------------------------------
/**
 * Tested API: Test Uninitialized state of - le_gnss_Enable(), le_gnss_Stop(), le_gnss_Start()
 *                                           le_gnss_GetTtff()
 *
 * Verify that behaves as expected
 */
//--------------------------------------------------------------------------------------------------
static void Testset_gnss_UninitializedState
(
    void
)
{
    uint32_t ttffPtr;
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_Enable()));
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_Stop()));
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_Start()));
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_GetTtff(&ttffPtr)));
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: Device Active state.
 *
 * Tested API: le_gnss_GetState(),le_gnss_Disable(),le_gnss_Start(),le_gnss_SetConstellation(),
 *        le_gnss_GetConstellation(),le_gnss_SetConstellationArea(),le_gnss_GetConstellationArea(),
 *        le_gnss_GetAcquisitionRate(),le_gnss_SetAcquisitionRate(),le_gnss_GetNmeaSentences(),
 *        le_gnss_SetNmeaSentences(),le_gnss_SetMinElevation(),le_gnss_GetMinElevation()
 *
 * Verify the behaves as expected in failure and success.
 */
//--------------------------------------------------------------------------------------------------
static void Testle_gnss_DeviceActiveStateTest
(
    void
)
{
    uint32_t acqRate = 0;
    uint8_t  minElevation;
    le_result_t result;

    le_gnss_ConstellationBitMask_t constellationMask;
    le_gnss_NmeaBitMask_t nmeaMask = 0;
    le_gnss_ConstellationArea_t constellationArea;

    // GNSS device enabled by default
    LE_ASSERT(LE_GNSS_STATE_ACTIVE == (le_gnss_GetState()));
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_Disable()));

    // Check Disabled state
    LE_ASSERT(LE_DUPLICATE == (le_gnss_Start()));
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_SetConstellation(LE_GNSS_CONSTELLATION_GPS)));
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_GetConstellation(&constellationMask)));
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_SetConstellationArea(LE_GNSS_SV_CONSTELLATION_GPS,
                                                                LE_GNSS_WORLDWIDE_AREA)));
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_GetConstellationArea(LE_GNSS_SV_CONSTELLATION_GPS,
                                                                &constellationArea)));
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_SetConstellationArea(LE_GNSS_SV_CONSTELLATION_GLONASS,
                                                                LE_GNSS_WORLDWIDE_AREA)));
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_GetConstellationArea(LE_GNSS_SV_CONSTELLATION_GLONASS,
                                                                &constellationArea)));
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_GetAcquisitionRate(&acqRate)));
    LE_ASSERT(LE_FAULT == (le_gnss_GetAcquisitionRate(NULL)));
    acqRate = 10;
    result = le_gnss_SetAcquisitionRate(acqRate);
    LE_ASSERT((LE_NOT_PERMITTED == result)||(LE_OUT_OF_RANGE == result));
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_SetNmeaSentences(nmeaMask)));
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_GetNmeaSentences(&nmeaMask)));

    // test le_gnss_Get/SetMinElevation when GNSS device is disabled and the engine is not started.
    minElevation = 40;
    LE_ASSERT_OK(le_gnss_SetMinElevation(minElevation));
    LE_ASSERT_OK(le_gnss_GetMinElevation(&minElevation));
    LE_ASSERT(minElevation == 40);
    minElevation = 91;
    result = le_gnss_SetMinElevation(minElevation);
    LE_ASSERT(LE_OUT_OF_RANGE == result);
    LE_ASSERT(LE_FAULT == (le_gnss_GetMinElevation(NULL)));

    LE_ASSERT(LE_FAULT == (le_gnss_ForceHotRestart()));
    LE_ASSERT_OK(le_gnss_Start());
    LE_ASSERT(LE_FAULT == (le_gnss_ForceWarmRestart()));
    LE_ASSERT_OK(le_gnss_Start());
    LE_ASSERT(LE_FAULT == (le_gnss_ForceColdRestart()));
    LE_ASSERT_OK(le_gnss_Start());
    LE_ASSERT(LE_FAULT == (le_gnss_ForceFactoryRestart()));
    LE_ASSERT_OK(le_gnss_Start());
    LE_ASSERT(LE_GNSS_STATE_ACTIVE == (le_gnss_GetState()));

    LE_ASSERT_OK(le_gnss_Stop());
    LE_ASSERT(LE_GNSS_STATE_READY == (le_gnss_GetState()));
    LE_ASSERT_OK(le_gnss_Start());
    LE_ASSERT(LE_GNSS_STATE_ACTIVE == (le_gnss_GetState()));

    LE_ASSERT((le_gnss_Disable()) == LE_NOT_PERMITTED);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: Device Ready state.
 *
 * Tested API: le_gnss_GetState(),le_gnss_Disable(),le_gnss_Start(),le_gnss_SetConstellation(),
 *        le_gnss_GetConstellation(),le_gnss_SetConstellationArea(),le_gnss_GetConstellationArea(),
 *        le_gnss_GetAcquisitionRate(),le_gnss_SetAcquisitionRate(),le_gnss_GetNmeaSentences(),
 *        le_gnss_SetNmeaSentences(),le_gnss_SetMinElevation(),le_gnss_GetMinElevation()
 *
 * Verify the behaves as expected in failure and success.
 */
//--------------------------------------------------------------------------------------------------
static void Testle_gnss_DeviceReadyStateTest
(
    void
)
{
    uint32_t ttffValue;
    uint32_t acqRate;
    uint8_t  minElevation;

    le_gnss_ConstellationBitMask_t constellationMask;
    le_gnss_NmeaBitMask_t nmeaMask = 0;
    le_gnss_ConstellationArea_t constellationArea;

    LE_ASSERT_OK(le_gnss_SetConstellation(LE_GNSS_CONSTELLATION_GPS));
    LE_ASSERT_OK(le_gnss_GetConstellation(&constellationMask));
    LE_ASSERT(LE_FAULT == (le_gnss_GetConstellation(NULL)));

    LE_ASSERT_OK(le_gnss_SetConstellationArea(LE_GNSS_SV_CONSTELLATION_GALILEO,
                                              LE_GNSS_OUTSIDE_US_AREA));
    LE_ASSERT(LE_FAULT == (le_gnss_GetConstellationArea(LE_GNSS_SV_CONSTELLATION_GALILEO,
                                                        &constellationArea)));

    LE_ASSERT_OK(le_gnss_SetConstellationArea(LE_GNSS_SV_CONSTELLATION_GALILEO,
                                              LE_GNSS_WORLDWIDE_AREA));
    LE_ASSERT(LE_FAULT == (le_gnss_GetConstellationArea(LE_GNSS_SV_CONSTELLATION_GALILEO,
                                                        &constellationArea)));

    LE_ASSERT(LE_FAULT == (le_gnss_GetAcquisitionRate(&acqRate)));
    acqRate = 0;
    LE_ASSERT(LE_OUT_OF_RANGE == (le_gnss_SetAcquisitionRate(acqRate)));
    acqRate = 1100;
    LE_ASSERT_OK(le_gnss_SetAcquisitionRate(acqRate));
    LE_ASSERT(LE_FAULT == (le_gnss_GetNmeaSentences(NULL)));
    LE_ASSERT_OK(le_gnss_GetNmeaSentences(&nmeaMask));

    nmeaMask = LE_GNSS_NMEA_SENTENCES_MAX + 1;
    LE_ASSERT(LE_BAD_PARAMETER == (le_gnss_SetNmeaSentences(nmeaMask)));
    nmeaMask = 1100;
    LE_ASSERT_OK(le_gnss_SetNmeaSentences(nmeaMask));

    //test le_gnss_Get/SetMinElevation when GNSS device is in ready state.
    minElevation = 0;
    LE_ASSERT_OK(le_gnss_SetMinElevation(minElevation));
    LE_ASSERT_OK(le_gnss_GetMinElevation(&minElevation));
    LE_INFO("GNSS min elevation obtained: %d",minElevation);
    LE_ASSERT(minElevation == 0);

    //In Ready state test  force reset not permitted
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_ForceHotRestart()));
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_ForceWarmRestart()));
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_ForceColdRestart()));
    LE_ASSERT(LE_NOT_PERMITTED == (le_gnss_ForceFactoryRestart()));
    LE_ASSERT(LE_DUPLICATE == (le_gnss_Enable()));
    LE_ASSERT(LE_FAULT == (le_gnss_GetTtff(&ttffValue)));
    LE_ASSERT(LE_DUPLICATE == (le_gnss_Stop()));

    //When state is ready check disable/enable.
    LE_ASSERT(LE_GNSS_STATE_READY == (le_gnss_GetState()));
    LE_ASSERT_OK(le_gnss_Disable());
    LE_ASSERT(LE_GNSS_STATE_DISABLED == (le_gnss_GetState()));
    LE_ASSERT(LE_DUPLICATE == (le_gnss_Disable()));

    LE_ASSERT_OK(le_gnss_Enable());
    LE_ASSERT(LE_GNSS_STATE_READY == (le_gnss_GetState()));

    // Check for gnss init
    LE_ASSERT(LE_NOT_PERMITTED == (gnss_Init()));

    // Start GNSS device (ACTIVE state)
    LE_ASSERT_OK(le_gnss_Start());
    LE_ASSERT(LE_GNSS_STATE_ACTIVE == (le_gnss_GetState()));
    LE_ASSERT(LE_DUPLICATE == (le_gnss_Start()));
}

//--------------------------------------------------------------------------------------------------
/**
 * main of the test
 *
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_INFO("======== Start GNSS UnitTest ========");

    LE_INFO("======== GNSS Uninitilize state test========");
    Testset_gnss_UninitializedState();

    LE_INFO("======== GNSS Position Handler Test========");
    Testle_gnss_AddHandlers();

    LE_INFO("======== GNSS Position Fill the position data========");
    Testset_gnss_PositionData();

    LE_INFO("======== GNSS Device State Test========");
    Testle_gnss_GetState();

    LE_INFO("======== GNSS Device Ready State Test ========");
    Testle_gnss_DeviceReadyStateTest();

    LE_INFO("======== GNSS Device Active State Test ========");
    Testle_gnss_DeviceActiveStateTest();

    LE_INFO("======== GNSS Device Get LastSample ref ========");
    Testle_gnss_GetLastSampleRef();

    LE_INFO("======== GNSS Device SuplCertificate ========");
    Testle_gnss_SuplCertificate();

    LE_INFO("======== GNSS Device GetSbasConstellationCategory ========");
    Testle_gnss_GetSbasConstellationCategory();

    LE_INFO("======== GNSS EnableExtendedEphemerisFile========");
    Testle_gnss_EnableDisableLoadExtendedEphemerisFile();

    LE_INFO("======== GNSS GetExtendedEphemerisValidity========");
    Testle_gnss_GetExtendedEphemerisValidity();

    LE_INFO("======== GNSS InjectUtcTime========");
    Testle_gnss_InjectUtcTime();

    LE_INFO("======== GNSS SetSuplAssistedMode========");
    Testle_gnss_SetSuplAssistedMode();

    LE_INFO("======== GNSS Remove Position Handler========");
    Testle_gnss_RemoveHandlers();

    exit(EXIT_SUCCESS);
}
