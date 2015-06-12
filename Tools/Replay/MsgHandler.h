#ifndef AP_MSGHANDLER_H
#define AP_MSGHANDLER_H

#include <DataFlash.h>
#include <VehicleType.h>

#include <stdio.h>

#define LOGREADER_MAX_FIELDS 30

#define streq(x, y) (!strcmp(x, y))

class MsgHandler {
public:
    // constructor - create a parser for a MavLink message format
    MsgHandler(const struct log_Format &f);

    // retrieve a comma-separated list of all labels
    void string_for_labels(char *buffer, uint bufferlen);

    // field_value - retrieve the value of a field from the supplied message
    // these return false if the field was not found
    template<typename R>
    bool field_value(uint8_t *msg, const char *label, R &ret);

    bool field_value(uint8_t *msg, const char *label, Vector3f &ret);
    bool field_value(uint8_t *msg, const char *label,
		     char *buffer, uint8_t bufferlen);
    
    template <typename R>
    void require_field(uint8_t *msg, const char *label, R &ret)
        {   
            if (! field_value(msg, label, ret)) {
                field_not_found(msg, label);
            }
        }
    void require_field(uint8_t *msg, const char *label, char *buffer, uint8_t bufferlen);
    float require_field_float(uint8_t *msg, const char *label);
    uint8_t require_field_uint8_t(uint8_t *msg, const char *label);
    int32_t require_field_int32_t(uint8_t *msg, const char *label);
    uint16_t require_field_uint16_t(uint8_t *msg, const char *label);
    int16_t require_field_int16_t(uint8_t *msg, const char *label);

private:

    void add_field(const char *_label, uint8_t _type, uint8_t _offset,
                   uint8_t length);

    template<typename R>
    void field_value_for_type_at_offset(uint8_t *msg, uint8_t type,
                                        uint8_t offset, R &ret);

    struct format_field_info { // parsed field information
        char *label;
        uint8_t type;
        uint8_t offset;
        uint8_t length;
    };
    struct format_field_info field_info[LOGREADER_MAX_FIELDS];

    uint8_t next_field;
    size_t size_for_type_table[52]; // maps field type (e.g. 'f') to e.g 4 bytes

    struct format_field_info *find_field_info(const char *label);

    void parse_format_fields();
    void init_field_types();
    void add_field_type(char type, size_t size);
    uint8_t size_for_type(char type);
    void field_not_found(uint8_t *msg, const char *label);

protected:
    struct log_Format f; // the format we are a parser for
    ~MsgHandler();

    void location_from_msg(uint8_t *msg, Location &loc, const char *label_lat,
			   const char *label_long, const char *label_alt);

    void ground_vel_from_msg(uint8_t *msg,
			     Vector3f &vel,
			     const char *label_speed,
			     const char *label_course,
			     const char *label_vz);

    void attitude_from_msg(uint8_t *msg,
			   Vector3f &att,
			   const char *label_roll,
			   const char *label_pitch,
			   const char *label_yaw);
};

class LR_MsgHandler : public MsgHandler {
public:
    LR_MsgHandler(struct log_Format &f,
                  DataFlash_Class &_dataflash,
                  uint64_t &last_timestamp_usec);
    virtual void process_message(uint8_t *msg) = 0;
    bool set_parameter(const char *name, float value);

protected:
    DataFlash_Class &dataflash;
    void wait_timestamp(uint32_t timestamp);
    void wait_timestamp_usec(uint64_t timestamp);
    void wait_timestamp_from_msg(uint8_t *msg);

    uint64_t &last_timestamp_usec;

};


template<typename R>
bool MsgHandler::field_value(uint8_t *msg, const char *label, R &ret)
{
    struct format_field_info *info = find_field_info(label);
    if (info == NULL) {
        return false;
    }

    uint8_t offset = info->offset;
    if (offset == 0) {
        return false;
    }

    field_value_for_type_at_offset(msg, info->type, offset, ret);

    return true;
}


template<typename R>
inline void MsgHandler::field_value_for_type_at_offset(uint8_t *msg,
                                                      uint8_t type,
                                                      uint8_t offset,
                                                      R &ret)
{
    /* we register the types - add_field_type - so can we do without
     * this switch statement somehow? */
    switch (type) {
    case 'B':
        ret = (R)(((uint8_t*)&msg[offset])[0]);
        break;
    case 'c':
    case 'h':
        ret = (R)(((int16_t*)&msg[offset])[0]);
        break;
    case 'H':
        ret = (R)(((uint16_t*)&msg[offset])[0]);
        break;
    case 'C':
        ret = (R)(((uint16_t*)&msg[offset])[0]);
        break;
    case 'f':
        ret = (R)(((float*)&msg[offset])[0]);
        break;
    case 'I':
    case 'E':
        ret = (R)(((uint32_t*)&msg[offset])[0]);
        break;
    case 'L':
    case 'e':
        ret = (R)(((int32_t*)&msg[offset])[0]);
        break;
    case 'q':
        ret = (R)(((int64_t*)&msg[offset])[0]);
        break;
    case 'Q':
        ret = (R)(((uint64_t*)&msg[offset])[0]);
        break;
    default:
        ::printf("Unhandled format type (%c)\n", type);
        exit(1);
    }
}


/* subclasses below this point */

class LR_MsgHandler_AHR2 : public LR_MsgHandler
{
public:
    LR_MsgHandler_AHR2(log_Format &_f, DataFlash_Class &_dataflash,
                    uint64_t &_last_timestamp_usec, Vector3f &_ahr2_attitude)
        : LR_MsgHandler(_f, _dataflash,_last_timestamp_usec),
          ahr2_attitude(_ahr2_attitude) { };

    virtual void process_message(uint8_t *msg);

private:
    Vector3f &ahr2_attitude;
};


class LR_MsgHandler_ARM : public LR_MsgHandler
{
public:
    LR_MsgHandler_ARM(log_Format &_f, DataFlash_Class &_dataflash,
                   uint64_t &_last_timestamp_usec)
        : LR_MsgHandler(_f, _dataflash, _last_timestamp_usec) { };

    virtual void process_message(uint8_t *msg);
};


class LR_MsgHandler_ARSP : public LR_MsgHandler
{
public:
    LR_MsgHandler_ARSP(log_Format &_f, DataFlash_Class &_dataflash,
		    uint64_t &_last_timestamp_usec, AP_Airspeed &_airspeed) :
	LR_MsgHandler(_f, _dataflash, _last_timestamp_usec), airspeed(_airspeed) { };

    virtual void process_message(uint8_t *msg);

private:
    AP_Airspeed &airspeed;
};

class LR_MsgHandler_FRAM : public LR_MsgHandler
{
public:
    LR_MsgHandler_FRAM(log_Format &_f, DataFlash_Class &_dataflash,
		    uint64_t &_last_timestamp_usec) :
	LR_MsgHandler(_f, _dataflash, _last_timestamp_usec) { };

    virtual void process_message(uint8_t *msg);
};


class LR_MsgHandler_ATT : public LR_MsgHandler
{
public:
    LR_MsgHandler_ATT(log_Format &_f, DataFlash_Class &_dataflash,
                   uint64_t &_last_timestamp_usec, Vector3f &_attitude)
        : LR_MsgHandler(_f, _dataflash, _last_timestamp_usec), attitude(_attitude)
        { };
    virtual void process_message(uint8_t *msg);

private:
    Vector3f &attitude;
};


class LR_MsgHandler_BARO : public LR_MsgHandler
{
public:
    LR_MsgHandler_BARO(log_Format &_f, DataFlash_Class &_dataflash,
                    uint64_t &_last_timestamp_usec, AP_Baro &_baro)
        : LR_MsgHandler(_f, _dataflash, _last_timestamp_usec), baro(_baro) { };

    virtual void process_message(uint8_t *msg);

private:
    AP_Baro &baro;
};


class LR_MsgHandler_Event : public LR_MsgHandler
{
public:
    LR_MsgHandler_Event(log_Format &_f, DataFlash_Class &_dataflash,
                   uint64_t &_last_timestamp_usec)
        : LR_MsgHandler(_f, _dataflash, _last_timestamp_usec) { };

    virtual void process_message(uint8_t *msg);
};




class LR_MsgHandler_GPS_Base : public LR_MsgHandler
{

public:
    LR_MsgHandler_GPS_Base(log_Format &_f, DataFlash_Class &_dataflash,
                           uint64_t &_last_timestamp_usec, AP_GPS &_gps,
                           uint32_t &_ground_alt_cm, float &_rel_altitude)
        : LR_MsgHandler(_f, _dataflash, _last_timestamp_usec),
          gps(_gps), ground_alt_cm(_ground_alt_cm),
          rel_altitude(_rel_altitude) { };

protected:
    void update_from_msg_gps(uint8_t imu_offset, uint8_t *data, bool responsible_for_relalt);

private:
    AP_GPS &gps;
    uint32_t &ground_alt_cm;
    float &rel_altitude;
};


class LR_MsgHandler_GPS : public LR_MsgHandler_GPS_Base
{
public:
    LR_MsgHandler_GPS(log_Format &_f, DataFlash_Class &_dataflash,
                   uint64_t &_last_timestamp_usec, AP_GPS &_gps,
                   uint32_t &_ground_alt_cm, float &_rel_altitude)
        : LR_MsgHandler_GPS_Base(_f, _dataflash,_last_timestamp_usec,
                              _gps, _ground_alt_cm, _rel_altitude),
          gps(_gps), ground_alt_cm(_ground_alt_cm), rel_altitude(_rel_altitude) { };

    void process_message(uint8_t *msg);

private:
    AP_GPS &gps;
    uint32_t &ground_alt_cm;
    float &rel_altitude;
};

// it would be nice to use the same parser for both GPS message types
// (and other packets, too...).  I*think* the contructor can simply
// take e.g. &gps[1]... problems are going to arise if we don't
// actually have that many gps' compiled in!
class LR_MsgHandler_GPS2 : public LR_MsgHandler_GPS_Base
{
public:
    LR_MsgHandler_GPS2(log_Format &_f, DataFlash_Class &_dataflash,
                    uint64_t &_last_timestamp_usec, AP_GPS &_gps,
                    uint32_t &_ground_alt_cm, float &_rel_altitude)
        : LR_MsgHandler_GPS_Base(_f, _dataflash, _last_timestamp_usec,
                                 _gps, _ground_alt_cm,
                                 _rel_altitude), gps(_gps),
          ground_alt_cm(_ground_alt_cm), rel_altitude(_rel_altitude) { };
    virtual void process_message(uint8_t *msg);
private:
    AP_GPS &gps;
    uint32_t &ground_alt_cm;
    float &rel_altitude;
};



class LR_MsgHandler_IMU_Base : public LR_MsgHandler
{
public:
    LR_MsgHandler_IMU_Base(log_Format &_f, DataFlash_Class &_dataflash,
                        uint64_t &_last_timestamp_usec,
                        uint8_t &_accel_mask, uint8_t &_gyro_mask,
                        AP_InertialSensor &_ins) :
        LR_MsgHandler(_f, _dataflash, _last_timestamp_usec),
        accel_mask(_accel_mask),
        gyro_mask(_gyro_mask),
        ins(_ins) { };
    void update_from_msg_imu(uint8_t gps_offset, uint8_t *msg);

private:
    uint8_t &accel_mask;
    uint8_t &gyro_mask;
    AP_InertialSensor &ins;
};

class LR_MsgHandler_IMU : public LR_MsgHandler_IMU_Base
{
public:
    LR_MsgHandler_IMU(log_Format &_f, DataFlash_Class &_dataflash,
                   uint64_t &_last_timestamp_usec,
                   uint8_t &_accel_mask, uint8_t &_gyro_mask,
                   AP_InertialSensor &_ins)
        : LR_MsgHandler_IMU_Base(_f, _dataflash, _last_timestamp_usec,
                              _accel_mask, _gyro_mask, _ins) { };

    void process_message(uint8_t *msg);
};

class LR_MsgHandler_IMU2 : public LR_MsgHandler_IMU_Base
{
public:
    LR_MsgHandler_IMU2(log_Format &_f, DataFlash_Class &_dataflash,
                    uint64_t &_last_timestamp_usec,
                    uint8_t &_accel_mask, uint8_t &_gyro_mask,
                    AP_InertialSensor &_ins)
        : LR_MsgHandler_IMU_Base(_f, _dataflash, _last_timestamp_usec,
                              _accel_mask, _gyro_mask, _ins) {};

    virtual void process_message(uint8_t *msg);
};

class LR_MsgHandler_IMU3 : public LR_MsgHandler_IMU_Base
{
public:
    LR_MsgHandler_IMU3(log_Format &_f, DataFlash_Class &_dataflash,
                    uint64_t &_last_timestamp_usec,
                    uint8_t &_accel_mask, uint8_t &_gyro_mask,
                    AP_InertialSensor &_ins)
        : LR_MsgHandler_IMU_Base(_f, _dataflash, _last_timestamp_usec,
                              _accel_mask, _gyro_mask, _ins) {};

    virtual void process_message(uint8_t *msg);
};



class LR_MsgHandler_MAG_Base : public LR_MsgHandler
{
public:
    LR_MsgHandler_MAG_Base(log_Format &_f, DataFlash_Class &_dataflash,
                        uint64_t &_last_timestamp_usec, Compass &_compass)
	: LR_MsgHandler(_f, _dataflash, _last_timestamp_usec), compass(_compass) { };

protected:
    void update_from_msg_compass(uint8_t compass_offset, uint8_t *msg);

private:
    Compass &compass;
};

class LR_MsgHandler_MAG : public LR_MsgHandler_MAG_Base
{
public:
    LR_MsgHandler_MAG(log_Format &_f, DataFlash_Class &_dataflash,
                   uint64_t &_last_timestamp_usec, Compass &_compass)
        : LR_MsgHandler_MAG_Base(_f, _dataflash, _last_timestamp_usec,_compass) {};

    virtual void process_message(uint8_t *msg);
};

class LR_MsgHandler_MAG2 : public LR_MsgHandler_MAG_Base
{
public:
    LR_MsgHandler_MAG2(log_Format &_f, DataFlash_Class &_dataflash,
                    uint64_t &_last_timestamp_usec, Compass &_compass)
        : LR_MsgHandler_MAG_Base(_f, _dataflash, _last_timestamp_usec,_compass) {};

    virtual void process_message(uint8_t *msg);
};



class LR_MsgHandler_MSG : public LR_MsgHandler
{
public:
    LR_MsgHandler_MSG(log_Format &_f, DataFlash_Class &_dataflash,
                   uint64_t &_last_timestamp_usec,
                   VehicleType::vehicle_type &_vehicle, AP_AHRS &_ahrs) :
        LR_MsgHandler(_f, _dataflash, _last_timestamp_usec),
        vehicle(_vehicle), ahrs(_ahrs) { }


    virtual void process_message(uint8_t *msg);

private:
    VehicleType::vehicle_type &vehicle;
    AP_AHRS &ahrs;
};


class LR_MsgHandler_NTUN_Copter : public LR_MsgHandler
{
public:
    LR_MsgHandler_NTUN_Copter(log_Format &_f, DataFlash_Class &_dataflash,
			   uint64_t &_last_timestamp_usec, Vector3f &_inavpos)
	: LR_MsgHandler(_f, _dataflash, _last_timestamp_usec), inavpos(_inavpos) {};

    virtual void process_message(uint8_t *msg);

private:
    Vector3f &inavpos;
};


class LR_MsgHandler_PARM : public LR_MsgHandler
{
public:
    LR_MsgHandler_PARM(log_Format &_f, DataFlash_Class &_dataflash, uint64_t &_last_timestamp_usec) : LR_MsgHandler(_f, _dataflash, _last_timestamp_usec) {};

    virtual void process_message(uint8_t *msg);
};


class LR_MsgHandler_SIM : public LR_MsgHandler
{
public:
    LR_MsgHandler_SIM(log_Format &_f, DataFlash_Class &_dataflash,
                   uint64_t &_last_timestamp_usec,
                   Vector3f &_sim_attitude)
        : LR_MsgHandler(_f, _dataflash, _last_timestamp_usec),
          sim_attitude(_sim_attitude)
        { };

    virtual void process_message(uint8_t *msg);

private:
    Vector3f &sim_attitude;
};

#endif
