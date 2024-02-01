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
void GSM_soft_reset();
void restart_GSM();
void enableGPRS();
void flushSerial();
void SMS_read_all();
void check_SMS_command(String msg_str);
void run_USSD(String info);
int arr_size(String *str);
String *string_separator(String str, char delimiter);

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
    Serial.print("GSM CONFIG SET PIN: ");
    Serial.println(PIN);
    Serial.print("Length of PIN");
    Serial.println(strlen(PIN));
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
        GPRS_CONNECTED = false;
        return GPRS_CONNECTED;
    }
    GPRS_CONNECTED = true;
    return GPRS_CONNECTED;
}

void GSM_soft_reset()
{

    fona.enableGPRS(false); // basically shut down GPRS service

    if (!fona.sendCheckReply(F("AT+CFUN=1"), F("OK")))
    {
        Serial.println("Soft resetting GSM with full functionality failed!");
        return;
    }

    if (!GSM_init(fonaSerial))
    {
        Serial.println("GSM not fully configured");
        Serial.print("Failure point: ");
        Serial.println(GSM_INIT_ERROR);
        Serial.println();
    }
}

/***
 * ? Called 3 times. Review the impelementation of this
 * Todo: Change implementation to shut down GSM and then call GSM_init();
 *
 *
 ***/
void restart_GSM()
{

    flushSerial();

    // ToDO: Check if RST pin is physically connected to the board to determine eith a hard or soft reset

    // ! First version of noise PCB has no physical connection to the SIM900 external reset pin
    // ! Pin 4 of the ESP-12E is thus floating and is only declared to instantiate a FONA class
    // ** For this reason, a soft rest is more approriate. Fona::begin will literally write to an non-existent pin
    GSM_soft_reset();
}

void enableGPRS()
{
    // fona.setGPRSNetworkSettings(FONAFlashStringPtr(gprs_apn), FONAFlashStringPtr(gprs_username), FONAFlashStringPtr(gprs_password));

    int retry_count = 0;
    while ((fona.GPRSstate() != GPRS_CONNECTED) && (retry_count < 40))
    {
        delay(3000);
        fona.enableGPRS(true);
        retry_count++;
    }

    fona.enableGPRS(true);
}

void disableGPRS()
{
    fona.enableGPRS(false);
    delay(3000);
}

/*****************************************************************
/* flushSerial                                                   *
/*****************************************************************/
void flushSerial()
{
    while (fonaSS.available())
        fonaSS.read();
}

/***
 * ! SMS functionality still under development
 *
 *
 */

void SMS_read_all()
{

    // read all SMS
    int8_t smsnum = fona.getNumSMS();
    uint16_t smslen;
    int8_t smsn;

    char sms_msg[255];

    if ((fona.type() == FONA3G_A) || (fona.type() == FONA3G_E))
    {
        smsn = 0; // zero indexed
        smsnum--;
    }
    else
    {
        smsn = 1; // 1 indexed
    }

    for (; smsn <= smsnum; smsn++)
    {
        Serial.print(F("\n\rReading SMS #"));
        Serial.println(smsn);
        if (!fona.readSMS(smsn, sms_msg, 250, &smslen))
        { // pass in buffer and max len!
            Serial.println(F("Failed to read SMS!"));
            break;
        }
        // if the length is zero, its a special case where the index number is higher
        // so increase the max we'll look at!
        if (smslen == 0)
        {
            Serial.println(F("[empty slot]"));
            smsnum++;
            continue;
        }

        Serial.print(F("***** SMS #"));
        Serial.print(smsn);
        Serial.print(" (");
        Serial.print(smslen);
        Serial.println(F(") bytes *****"));
        Serial.println(sms_msg);
        Serial.println(F("*****"));

        // ToDo: Check if message contains a command
        //  this will change index if cmd prompts expecting another sms(es)

        // String sms(sms_msg);
        check_SMS_command(sms_msg);
    }
}

void check_SMS_command(String msg_str)
{
    int index_start = msg_str.indexOf("CMD_TYPE:");
    int index_end = msg_str.indexOf("CMD_END");

    if (index_start == -1 || index_end == -1)
    {
        Serial.println("No command found or Wrong command formart");
        return;
    }

    else
    {

        String start_str = "CMD_TYPE:";
        msg_str = msg_str.substring(msg_str.indexOf(start_str) + start_str.length(), index_end);
        Serial.println(msg_str);
        String CMD_TYPE = msg_str;                               // copy message string
        CMD_TYPE = CMD_TYPE.substring(0, CMD_TYPE.indexOf("=")); // Extract the string that bears the command just before the first '='
        // Serial.println(CMD_TYPE);

        if (CMD_TYPE = "USSD")
        {
            // run USSD code
            Serial.println("Attempt to run USSD code");
            run_USSD(msg_str);
            return;
        }

        else
        {
            Serial.print("Command ");
            Serial.print(CMD_TYPE);
            Serial.println(" not found or supported");
            return;
        }
    }
}

void run_USSD(String info)
{
    Serial.print("Received info: ");
    Serial.println(info);

    String *SEPARATED_CMD = string_separator(info, ';');
    Serial.print("Size of SEPARATED CMD: ");
    Serial.println(sizeof(SEPARATED_CMD));

    for (byte i = 0; i < arr_size(SEPARATED_CMD); i++)
    {
        Serial.println(SEPARATED_CMD[i]);
    }

    String USSD = SEPARATED_CMD[0].substring(SEPARATED_CMD[0].indexOf("=") + 1);
    String target = SEPARATED_CMD[1].substring(SEPARATED_CMD[1].indexOf("=") + 1);
    String pattern = SEPARATED_CMD[2].substring(SEPARATED_CMD[2].indexOf("=") + 1);

    delete[] SEPARATED_CMD;
    Serial.println("Extrated info from USSD message");
    Serial.println(USSD);
    Serial.println(target);
    Serial.println(pattern);

    String *USSD_seq = string_separator(USSD, ',');

    for (int i = 0; i <= arr_size(USSD_seq); i++)
    {
        // handle AT commands here
        Serial.println(USSD_seq[i]);
    }

    delete[] USSD_seq;

    // TODO: Check message with matching pattern and send to target number
}

int arr_size(String *str)
{
    int count = 0;
    while (str[count] != "\0")
    {
        count++;
    }

    return count;
}

String *string_separator(String str, char delimiter)
{ // note: pass delimiter using single quotes
    int arr_size = 12;
    String *separated_str = new String[arr_size];
    int arr_index = 0;

    while (str.indexOf(delimiter) != -1)
    {
        int delimiter_index = str.indexOf(delimiter);
        String extracted_code = str;
        extracted_code = extracted_code.substring(0, delimiter_index);
        str = str.substring(delimiter_index + 1);

        separated_str[arr_index] = extracted_code;
        arr_index++;
    }

    // separated_str[arr_index] = '\0';  // Does not seem to have an effect
    return separated_str;
}