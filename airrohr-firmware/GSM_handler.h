#include <SoftwareSerial.h>
#include <Adafruit_FONA.h>
#include "ext_def.h"

SoftwareSerial fonaSS(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

char SIM_PIN[5] = GSM_PIN;
bool GSM_CONNECTED = false;
bool SIM_AVAILABLE = false;
bool GPRS_CONNECTED = false;
bool SIM_PIN_SET = false;
bool SIM_USABLE = false;
char SIM_CID[21] = "";
String GSM_INIT_ERROR = "";
String NETWORK_NAME = "";

/***
 * ! Previous GSM Config
 * ****/
// uint8_t GSM_CONNECTED = 1;
// uint8_t GPRS_CONNECTED = 1;

// char gsm_pin[5] = "";

// char gprs_apn[100] = "internet";
// char gprs_username[100] = "";
// char gprs_password[100] = "";

/**** Function Declacrations **/
bool GSM_init(SoftwareSerial *gsm_serial);
static void unlock_pin(char *PIN);
String handle_AT_CMD(String cmd, int _delay = 1000);
void SIM_PIN_Setup();
bool is_SIMCID_valid();
bool GPRS_init();
// Set a decent delay before this to warm up the GSM module

bool GSM_init(SoftwareSerial *gsm_serial)
{ // Pass a ptr to SoftwareSerial GSM instance
    gsm_serial->begin(4800);
    String error_msg = "";
    // Check if there is serial communication with a GSM module
    if (!fona.begin(*gsm_serial))
    {
        error_msg = "Could not find GSM module";
        GSM_INIT_ERROR = error_msg;
        Serial.println(error_msg);
        GSM_CONNECTED = false;
        return false;
    }

    Serial.print("GSM module found!");

    // Check if SIM is inserted
    if (!is_SIMCID_valid())
    {
        error_msg = "Could not get SIM CID";
        GSM_INIT_ERROR = error_msg;
        Serial.println(error_msg);
        return false;
    }

    // SIM setup
    Serial.println("SIM card available");
    Serial.println("Setting up SIM..");

    SIM_PIN_Setup();

    if (!SIM_PIN_SET)
    {
        error_msg = "Unable to set SIM PIN";
        GSM_INIT_ERROR = error_msg;
        Serial.println(error_msg);

        return false;
    }
    // Set if SIM is usable flag
    SIM_USABLE = true;

    // Register to network
    bool registered_to_network = false;
    int retry_count = 0;
    while (!registered_to_network && retry_count < 10)
    {
        if (fona.getNetworkStatus() == 1)
            registered_to_network = true;

        retry_count++;
        delay(3000);
    }

    if (!registered_to_network)
    {
        error_msg = "Could not register to network";
        GSM_INIT_ERROR = error_msg;
        Serial.println(error_msg);
        return false;
    }

    // Set GPRS APN details
    fona.setGPRSNetworkSettings(F(GPRS_APN), F(GPRS_USERNAME), F(GPRS_PASSWORD));

    // Attempt to enable GPRS
    Serial.println("Attempting to enable GPRS");
    delay(2000);

    if (!fona.enableGPRS(true)) // ToDO: Have multiple attempts
    {
        error_msg = "Failed to enable GPRS";
        GSM_INIT_ERROR = error_msg;
        Serial.println(error_msg);
        return false;
    }

    Serial.print("GPRS enabled!");

    GPRS_CONNECTED = true;
    // ToDo: Attempt to do a ping test to determine whether we can communicate with the internet

    return true;
}

static void unlock_pin(char *PIN)
{
    // flushSerial();

    // Attempt to SET PIN if not empty
    if (strlen(PIN) > 1)
    {
        // debug_outln(F("\nAttempting to Unlock SIM please wait: "), DEBUG_MIN_INFO);
        Serial.print("Attempting to unlock SIM using PIN: ");
        Serial.println(PIN);
        if (!fona.unlockSIM(PIN))
        {
            // debug_outln(F("Failed to Unlock SIM card with pin: "), DEBUG_MIN_INFO);
            Serial.print("Failed to Unlock SIM card with PIN: ");
            // debug_outln(gsm_pin, DEBUG_MIN_INFO);
            Serial.println(PIN);
            SIM_PIN_SET = false;
            return;
        }

        SIM_PIN_SET = true;
    }
}

String handle_AT_CMD(String cmd, int _delay)
{
    while (Serial.available() > 0)
    {
        Serial.read();
    }
    String RESPONSE = "";
    fona.println(cmd);
    delay(_delay); // Avoid putting any code that might delay the receiving all contents from the serial buffer as it is quickly filled up

    while (fona.available() > 0)
    {
        RESPONSE += fona.readString();
    }
    // Serial.println();
    // Serial.println("GSM RESPONSE:");
    // Serial.println("-------");
    // Serial.print(RESPONSE);
    // Serial.println("-----");

    return RESPONSE;
}

void SIM_PIN_Setup()
{

    String res = handle_AT_CMD("AT+CPIN?");
    int start_index = res.indexOf(":");
    res = res.substring(start_index + 1);
    res.trim();
    Serial.print("PIN STATUS: ");
    Serial.println(res);
    if (res.startsWith("READY"))
    {
        SIM_PIN_SET = true;
        return;
    }

    else if (res.startsWith("SIM PIN"))
    {
        unlock_pin(SIM_PIN);
        return;
    }
    else if (res.startsWith("SIM PUK"))
    { // ToDo: Attempt to set PUK;
        return;
    }

    else
    {
        return;
    }
}

bool is_SIMCID_valid()
{
    char res[21];
    fona.getSIMCCID(res);
    String ccid = String(res);
    if (ccid.indexOf("ERROR") > -1) // Means string has the word error
    {
        SIM_AVAILABLE = false;
        return false;
    }

    else
    {
        strcpy(SIM_CID, res);
        SIM_AVAILABLE = true;
        Serial.print("SIM CCID: ");
        Serial.println(SIM_CID);
        return true;
    }
}

// Similar to FONA enableGPRS() but quicker because APN setting are not configured as it is configured during GSM_init()
bool GPRS_init()
{
    // handle_AT_CMD("AT+SAPBR=0,1"); // Disable GPRS
    handle_AT_CMD("AT+CGATT=1");                // Attach GPRS service
    String res = handle_AT_CMD("AT+SAPBR=1,1"); // Enable GPRS
    if (res.indexOf("OK") == -1)
    {
        Serial.println("Failed to enable GPRS");
        return false;
    }
    return true;
}