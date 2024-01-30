#include <SoftwareSerial.h>
#include <Adafruit_FONA.h>
// Change this appropriate GSM pins
#define FONA_TX 0
#define FONA_RX 2
#define FONA_RST 4

char SIM_PIN[5] = "";
#define GSM_APN ""
#define APN_USER ""
#define APN_PWD ""

SoftwareSerial fonaSS(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

bool GSM_CONNECTED = false;
bool SIM_AVAILABLE = false;
bool GPRS_CONNECTED = false;
bool SIM_PIN_SET = false;
char SIM_CID[21] = "";
String GSM_INIT_ERROR = "";
String NETWORK_NAME = "";

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
    char SIM_CCID[21];
    int SIM_CCID_LEN = fona.getSIMCCID(SIM_CCID);
    Serial.print("SIM CCID: ");
    Serial.println(SIM_CCID);

    if (SIM_CCID_LEN < 1)
    {
        error_msg = "SIM CARD NOT FOUND";
        GSM_INIT_ERROR = error_msg;
        Serial.println(error_msg);
        SIM_AVAILABLE = false;
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
    fona.setGPRSNetworkSettings(F(GSM_APN), F(APN_USER), F(APN_PWD));

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

String handle_AT_CMD(String cmd, int _delay = 1000)
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