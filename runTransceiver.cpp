#include <csignal>
#include <cstdint>
#include <ctime>
#include <random>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
//#include "cpptoml.h"

#include "radioDevice.h"
#include "Transceiver.h"


// FIXME: This "shutdown" method can stand to be cleaned up, but that's a small problem for later.
volatile bool gbShutdown = false;

static void ctrlCHandler(int signo) {
    SPDLOG_INFO("Received shutdown signal: {}", signo);
    gbShutdown = true;
}

int main(int argc, char *argv[]) {
    /*** Parse CLI Arguments ***/
    // TODO: Properly parse and handle any arguments

    /*** Parse Config File ***/
    // FIXME: Get the config file path from a command line arg.
    // FIXME: Update this application to use a config file.
//    shared_ptr<cpptoml::table> config = cpptoml::parse_file("/etc/openbts/smqueue.conf");
//    shared_ptr<cpptoml::table> config_smqueue = config->get_table("smqueue");
//    shared_ptr<cpptoml::table> config_logging = config->get_table("logging");
//    std::string log_level = *config_logging->get_as<std::string>("level");
//    std::string log_type = *config_logging->get_as<std::string>("type");
//    std::string log_filename = *config_logging->get_as<std::string>("filename");
    std::string log_level = "debug";
    std::string log_type = "console";
    std::string log_filename = "/var/log/smqueue.log";

    // FIXME: Figure out what this is supposed to be getting and move to config file.
    //        The `deviceArgs` string is sent to the radio class.
    std::string deviceArgs;
    if (argc == 3) {
        deviceArgs = std::string(argv[2]);
    } else {
        deviceArgs = "";
    }

    std::uint16_t trxPort = 5700;
    std::string trxAddr = "127.0.0.1";

    std::string clock_reference_str = "internal";

    /*** Setup Logger ***/
    // create color console logger if enabled
    if (log_type == "console") {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::from_str(log_level));
        //console_sink->set_pattern("[multi_sink_example] [%^%l%$] %v");
        auto console_logger = std::make_shared<spdlog::logger>("console_logger", console_sink);
        console_logger->set_level(spdlog::level::from_str(log_level));
        spdlog::register_logger(console_logger);
        spdlog::set_default_logger(console_logger);
    }
        // create file logger if enabled
    else if (log_type == "file") {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_filename);
        file_sink->set_level(spdlog::level::from_str(log_level));
        //file_sink->set_pattern("[multi_sink_example] [%^%l%$] %v");
        auto file_logger = std::make_shared<spdlog::logger>("file_logger", file_sink);
        file_logger->set_level(spdlog::level::from_str(log_level));
        spdlog::register_logger(file_logger);
        spdlog::set_default_logger(file_logger);
    }
    SPDLOG_WARN("Log level value from the config: {}", log_level);
    SPDLOG_DEBUG("DEBUG mode is enabled");

    /*** Setup Signal Handlers ***/
    if (signal(SIGINT, ctrlCHandler) == SIG_ERR) {
        SPDLOG_ERROR("Couldn't install signal handler for SIGINT");
        return EXIT_FAILURE;
    }

    if (signal(SIGTERM, ctrlCHandler) == SIG_ERR) {
        SPDLOG_ERROR("Couldn't install signal handler for SIGTERM");
        return EXIT_FAILURE;
    }

    /*** Seed the random number source ***/
    // FIXME: Moving to a better pseudo random number generator (pRNG).
    //        Should probably seed from the kernel/hardware RNG, not time.
    srandom(std::time(nullptr));

    // https://en.cppreference.com/w/cpp/numeric/random/mersenne_twister_engine
    std::mt19937 mrandom;
    mrandom.seed(std::time(nullptr));

    /*** Configure the Clock Source ***/
    RadioDevice::ReferenceType clock_reference;
    if (clock_reference_str == "gpsdo") {
        clock_reference = RadioDevice::REF_GPS;
        SPDLOG_INFO("Clock Reference: GPS");
    } else if (clock_reference_str == "external") {
        clock_reference = RadioDevice::REF_EXTERNAL;
        SPDLOG_INFO("Clock Reference: External");
    } else {
        clock_reference = RadioDevice::REF_INTERNAL;
        SPDLOG_INFO("Clock Reference: Internal");
    }

    /*** Setup the USRP SDR ***/
    /* Samples-per-symbol (SPS) for downlink path
     *     4 - Uses precision modulator (more computation, less distortion)
     *     1 - Uses minimized modulator (less computation, more distortion)
     *
     *     Other values are invalid. Receive path (uplink) is always
     *     downsampled to 1 sps
     */
    RadioDevice *usrp = RadioDevice::make(4);
    int radio_type = usrp->open(deviceArgs, clock_reference);
    if (radio_type < 0) {
        SPDLOG_ERROR("Bad USRP radio type.");
        return EXIT_FAILURE;
    }

    // FIXME: Not a fan of "failure booleans" to handle cleanups.  Will likely
    //        move this logic to a radio handler class.
    bool failure = false;

    RadioInterface *radio = nullptr;
    switch (radio_type) {
        case RadioDevice::NORMAL:
            //radio = new RadioInterface(usrp, 3, 4, false);
            radio = new RadioInterface(usrp, 3, 4); // Why was false being passed in instead of a GSM::Time?
            break;
        case RadioDevice::RESAMP_64M:
        case RadioDevice::RESAMP_100M:
            //radio = new RadioInterfaceResamp(usrp, 3, 4, false);
            radio = new RadioInterfaceResamp(usrp, 3, 4); // Why was false being passed in instead of a GSM::Time?
            break;
        default:
            SPDLOG_ERROR("Unsupported radio type configuration");
            failure = true;
            break;
    }

    if (!failure && !radio->init(radio_type)) {
        SPDLOG_ERROR("Failed to initialize radio interface");
        failure = true;
    }

    Transceiver *trx = nullptr;
    if (!failure) {
        trx = new Transceiver(trxPort, trxAddr.c_str(), 4, GsmTime(3, 0), radio);
        if (!trx->init()) {
            SPDLOG_ERROR("Failed to initialize transceiver");
            failure = true;
        }
    }

    if (!failure) {
        trx->receiveFIFO(radio->receiveFIFO());
        trx->start();
    }

    /*** Wait while the radio does its thing ***/
    while (!failure && !gbShutdown) {
        // FIXME: Should this be serviced more quickly?  Maybe 0.1 seconds?
        sleep(1);
    }

    /*** Shutdown the radio ***/
    SPDLOG_INFO("Shutting down transceiver");

    // FIXME: Should be able to do this with scopes and smart pointers, maybe a
    //        wrapper class that handles these radio parts.
    delete trx;
    delete radio;
    delete usrp;

    if (failure) {
        return EXIT_FAILURE;
    }

    return 0;
}
