#include "etherlab.h"

void fm_auto::DuetflEthercatController::signal_handler(int signum) {
//    fprintf(stderr,"signal_handler \n");
    switch (signum) {
        case SIGALRM:
//            sig_alarms++;
            break;
    case SIGINT:
        fprintf(stderr,"use ctrl+c ,need do something \n");
        // disable the operation
        // send 0x0002 to controlword
        uint16_t value = 0x0002;
        writeSdoControlword(value);
        break;
    }
}
void fm_auto::DuetflEthercatController::writeSdoControlword(uint16_t &value)
{
    static ec_sdo_request_t *slave0_sdo_controlword_write;
}

void fm_auto::DuetflEthercatController::my_sig_handler(int signum) {
    ROS_INFO("my_sig_handler <%d>\n",signum);
    switch (signum) {
        case SIGINT:
            fprintf(stderr,"use ctrl+c ,need do something before exit\n");
            // disable the operation
            // send 0x0002 to controlword
            disable_operation();
            exit(-1);
            break;
    }
}
void fm_auto::DuetflEthercatController::disable_operation()
{

}
fm_auto::DuetflEthercatController::DuetflEthercatController()
    : domain_input(NULL),domain_output(NULL),master(NULL)
{

}
bool fm_auto::DuetflEthercatController::init()
{
    FREQUENCY = 300; //hz

    ROS_INFO("Starting timer...\n");
    tv.it_interval.tv_sec = 0;
    tv.it_interval.tv_usec = 1000000 / FREQUENCY;
    tv.it_value.tv_sec = 0;
    tv.it_value.tv_usec = 1000;
    if (setitimer(ITIMER_REAL, &tv, NULL)) {
        ROS_ERROR("Failed to start timer: %s\n", strerror(errno));
        return false;
    }

    // init ethercat
    if(!initEthercat())
    {
        return false;
    }

    return true;
}
bool fm_auto::DuetflEthercatController::operateHomingMethod()
{
    // set homing_method to current position
    HOMING_METHOD hm = getMotorHomingMode(slave_zero);
    if(hm != HM_current_position)
    {
        ros::Time time_begin = ros::Time::now();
        while(hm != HM_current_position)
        {
            ROS_INFO_ONCE("homing method not current position: %d",hm);
            setMotorHomingMode(HM_current_position);
            hm = getMotorHomingMode(slave_zero);
        //        time_t t_n = time(0);   // get time now
        //        struct tm * now = localtime( & t );
        //        if(now_b->tm_sec - )
            ros::Time time_now = ros::Time::now();
            if( (time_now.toSec() - time_begin.toSec()) > 10 ) // 10 sec
            {
                break;
            }
        }
        if(getMotorHomingMode(slave_zero) != HM_current_position)
        {
            ROS_ERROR("cannot get slave0 homing method to current position");
            return false;
        }
    }
    // trigger home position
}

bool fm_auto::DuetflEthercatController::initSDOs()
{
    // slave0 sdo
    ROS_INFO("Creating operation mode read SDO requests...\n");
    if (!(fm_auto::slave0_sdo_operation_mode_display = ecrt_slave_config_create_sdo_request(fm_auto::slave_zero,
                                                                                            ADDRESS_MODES_OF_OPERATION_DISPLAY,
                                                                                            0, 1))) // uint8 data size 1
    {
        ROS_ERROR("Failed to create SDO modes_of_operation_display 0x6061 request.\n");
        return false;
    }
    slave0_operation_mode_display_fmsdo = new fm_sdo();
    slave0_operation_mode_display_fmsdo->sdo = fm_auto::slave0_sdo_operation_mode_display;
    slave0_operation_mode_display_fmsdo->descrption = "operation_mode_display 0x6061";

    ROS_INFO("Creating Homing Method read SDO requests...\n");
    if (!(fm_auto::slave0_sdo_homing_method = ecrt_slave_config_create_sdo_request(fm_auto::slave_zero,
                                                                                            ADDRESS_HOMING_METHOD,
                                                                                            0, 1))) // uint8 data size 1
    {
        ROS_ERROR("Failed to create SDO slave0_sdo_homing_methiod 0x6098 request.\n");
        return false;
    }
    slave0_homing_method_fmSdo = new fm_sdo();
    slave0_homing_method_fmSdo->sdo = fm_auto::slave0_sdo_homing_method;
    slave0_homing_method_fmSdo->descrption = "homing_method 0X6098";


    ROS_INFO( "Creating operation mode write SDO requests...\n");
    if (!(fm_auto::slave0_sdo_operation_mode_write = ecrt_slave_config_create_sdo_request(fm_auto::slave_zero,
                                                                                          ADDRESS_MODES_OF_OPERATION,
                                                                                          0, 1))) // uint8 data size 1
    {
        ROS_ERROR("Failed to create SDO MODES_OF_OPERATION request.\n");
        return -1;
    }

    ROS_INFO("Creating controlword write SDO requests...\n");
    if (!(fm_auto::slave0_sdo_controlword_write = ecrt_slave_config_create_sdo_request(fm_auto::slave_zero,
                                                                                       ADDRESS_CONTROLWORD,
                                                                                       0, 2))) // uint16 data size 2
    {
        ROS_ERROR("Failed to create SDO CONTROLWORD request.\n");
        return -1;
    }

    ROS_INFO("Creating statusword read SDO requests...\n");
    if (!(fm_auto::slave0_sdo_statusword_read = ecrt_slave_config_create_sdo_request(slave_zero, ADDRESS_STATUSWORD,
                                                                                     0, 2))) // uint16 data size 2
    {
        ROS_ERROR("Failed to create SDO STATUSWORD request.\n");
        return -1;
    }

    ecrt_sdo_request_timeout(fm_auto::slave0_sdo_operation_mode_display, 500); // ms
    ecrt_sdo_request_timeout(fm_auto::slave0_sdo_operation_mode_write, 500); // ms
    ecrt_sdo_request_timeout(fm_auto::slave0_sdo_controlword_write, 500); // ms
    ecrt_sdo_request_timeout(fm_auto::slave0_sdo_statusword_read, 500); // ms

    //TODO: slave1 sdo
}

bool fm_auto::DuetflEthercatController::initEthercat()
{
    // we only have one master,who is g...
    master = ecrt_request_master(0);
    if (!master)
    {
        return -1;
    }

//    fm_auto::slave_zero = NULL;
    fm_auto::slave_zero = ecrt_master_slave_config(master,SlaveZeroAliasAndPosition,VendorID_ProductCode);
    if(!fm_auto::slave_zero)
    {
        ROS_ERROR("Failed to get slave configuration.\n");
        return false;
    }

    if(!initSDOs())
    {
        ROS_ERROR("init sdos failed!\n");
        return false;
    }

    // two domains
    domain_input = ecrt_master_create_domain(master);
    domain_output = ecrt_master_create_domain(master);

    if (!domain_input || !domain_output)
    {
        ROS_ERROR("init domain failed!\n");
        return false;
    }

    ROS_INFO("Activating master...\n");
    if (ecrt_master_activate(master))
    {
        ROS_ERROR("active master failed!\n");
        return false;
    }
    // pdo domain data point
    if (!(domain_output_pd = ecrt_domain_data(domain_output))) {
        return false;
    }
    if (!(domain_input_pd = ecrt_domain_data(domain_input))) {
        return false;
    }

    // set pid priority
    pid_t pid = getpid();
    if (setpriority(PRIO_PROCESS, pid, -19))
    {
        ROS_ERROR("Warning: Failed to set priority: %s\n",
                strerror(errno));
    }

    // domain entry list
    if (ecrt_domain_reg_pdo_entry_list(domain_output, fm_auto::domain_output_regs)) {
        ROS_ERROR("Output PDO entry registration failed!\n");
        return false;
    }
    if (ecrt_domain_reg_pdo_entry_list(domain_input, fm_auto::domain_input_regs)) {
        ROS_ERROR("Input PDO entry registration failed!\n");
        return false;
    }

    // signal handler
    // handle ctrl+c important
    if (signal(SIGINT, fm_auto::DuetflEthercatController::my_sig_handler) == SIG_ERR)
    {
            ROS_ERROR("\ncan't catch SIGUSR1\n");
            return false;
    }

    // get alarm to control frequcy
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, 0)) {
        fprintf(stderr, "Failed to install signal handler!\n");
        return -1;
    }
}

void fm_auto::DuetflEthercatController::run()
{
    while (1) {
        pause();

        ros::spinOnce();

//        while (sig_alarms != user_alarms) {
            cyclic_task();
//            user_alarms++;
//        }
    }
}
bool fm_auto::DuetflEthercatController::sendOneReadSDO(fm_sdo *fmSdo_read)
{
    ecrt_master_receive(master);
    //  upload
    ecrt_sdo_request_read(fmSdo_read->sdo);
    ecrt_master_send(master);
}
bool fm_auto::DuetflEthercatController::sendOneWriteSDO(fm_sdo *fmSdo_write)
{
    ecrt_master_receive(master);
    // download
    ecrt_sdo_request_write(fmSdo_write->sdo);
    ecrt_master_send(master);
}
bool fm_auto::DuetflEthercatController::checkSDORequestState(fm_sdo *fmSdo)
{
    bool state=false;
    ecrt_master_receive(master);
    switch (ecrt_sdo_request_state(fmSdo->sdo)) {
        case EC_REQUEST_UNUSED: // request was not used yet
            ROS_INFO_ONCE("request was not used yet\n");
            break;
        case EC_REQUEST_BUSY:
            ROS_INFO_ONCE("request is busy...\n");
            break;
        case EC_REQUEST_SUCCESS:
            state=true;
            break;
        case EC_REQUEST_ERROR:
            ROS_ERROR("Failed to read SDO!\n");
            break;
    }
    return state;
}
fm_auto::HOMING_METHOD fm_auto::DuetflEthercatController::getMotorHomingMode(const ec_slave_config_t *slave_config)
{
    int8_t mode_value;
    //1. send read sdo request
    sendOneReadSDO(slave0_operation_mode_display_fmsdo);
    //2. check sdo state
    if(checkSDORequestState(slave0_operation_mode_display_fmsdo))
    {
        mode_value = EC_READ_S8(ecrt_sdo_request_data(slave0_operation_mode_display_fmsdo->sdo));
    }
    ROS_INFO_ONCE("operation_mode_display: %02x",mode_value);
//    if(mode_value == fm_auto::HM_current_position)
    return (fm_auto::HOMING_METHOD)mode_value;
}

bool fm_auto::DuetflEthercatController::setMotorHomingMode(fm_auto::HOMING_METHOD &hm)
{
    int8_t v=(int8_t)hm;
    EC_WRITE_S8(ecrt_sdo_request_data(slave0_operation_mode_display_fmsdo->sdo), v);
    ecrt_master_send(master);
}

void fm_auto::DuetflEthercatController::check_master_state()
{
    ec_master_state_t ms;

    ecrt_master_state(master, &ms);

    if (ms.slaves_responding != master_state.slaves_responding)
        printf("%u slave(s).\n", ms.slaves_responding);
    if (ms.al_states != master_state.al_states)
        printf("AL states: 0x%02X.\n", ms.al_states);
    if (ms.link_up != master_state.link_up)
        printf("Link is %s.\n", ms.link_up ? "up" : "down");

    master_state = ms;
}
void fm_auto::DuetflEthercatController::cyclic_task()
{
    // receive process data
    ecrt_master_receive(master);
    ecrt_domain_process(domain_output);
    ecrt_domain_process(domain_input);


    // check process data state (optional)
//    check_domain1_state();

    // check for master state (optional)
    check_master_state();

    // check for islave configuration state(s) (optional)
//    check_slave_config_states();


    // read PDO data
    readPDOsData();

    // send process data
    ecrt_domain_queue(domain_output);
    ecrt_domain_queue(domain_input);
    ecrt_master_send(master);
}
bool fm_auto::DuetflEthercatController::writePdoTargetPosition(int32_t &value)
{
    // TODO:check boundary
    // write process data
    EC_WRITE_U32(domain_output_pd + fm_auto::OFFSET_TARGET_POSITION, value);

    return true;
}
bool fm_auto::DuetflEthercatController::writePdoControlword(uint16_t &value)
{
    EC_WRITE_U16(domain_output_pd + fm_auto::OFFSET_CONTROLWORD, value);

    return true;
}

bool fm_auto::DuetflEthercatController::readPDOsData()
{
    printf("pdo statusword value: %04x offset %u\n",
            EC_READ_U16(domain_input_pd + OFFSET_STATUSWORD),OFFSET_STATUSWORD);
}

bool fm_auto::DuetflEthercatController::processSDOs()
{
    // check has sdos to send?
    std::list<fm_auto::fm_sdo*> sdoPool;
    if(!activeSdoPool.empty())
    {
        std::list<fm_sdo*>::iterator it;
        for(it=activeSdoPool.begin();it!=activeSdoPool.end();++it)
        {
            fm_auto::fm_sdo *fmSdo = *it;
            if(!fmSdo->isOperate) // never operate before
            {
                sdoPool.push_back(fmSdo);
                // check upload or download
                if(fmSdo->isReadSDO)
                {
                    ecrt_sdo_request_read(fmSdo->sdo);
                }
                else{
                    ecrt_sdo_request_write(fmSdo->sdo);
                }
                fmSdo->isOperate=true;
            }
            else
            {
                // check state
                switch (ecrt_sdo_request_state(fmSdo->sdo)) {
                    case EC_REQUEST_UNUSED: // request was not used yet
                        break;
                    case EC_REQUEST_BUSY:
        //                printf( "Still busy...\n");
                    sdoPool.push_back(fmSdo);
                        break;
                    case EC_REQUEST_SUCCESS:
                    // remove from list
                        fmSdo->isOperate = false;
                        fmSdo->callback();
                        break;
                    case EC_REQUEST_ERROR:
                        ROS_ERROR("Failed to operate SDO %s!\n",fmSdo->descrption.c_str());
                        break;
                }//switch
            }//else
        }//for
            activeSdoPool.clear();
            activeSdoPool = sdoPool;

    }//if
}