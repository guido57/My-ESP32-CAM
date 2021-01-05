#include "ESP32Ping.h"
#include "captive_portal.h"

#include <TaskScheduler.h>
Scheduler myScheduler;
//=== CaptivePortal stuff ================
String softAP_ssid = "";
String softAP_password  = APPSK;

/* hostname for mDNS. Should work at least on windows. Try http://esp32.local */
const char *myHostname = "esp32";

/* Don't set this wifi credentials. They are configurated at runtime and stored on EEPROM */
char ssid[33] = "";
char password[65] = "";

void saveCredentials();

/* Setup the Access Point */
void AccessPointSetup();

/* Soft AP network parameters */
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

/** Should I connect to WLAN asap? */
boolean connect;

/** Last time I tried to connect to WLAN */
unsigned long lastConnectTry = 0;

/** Last time I tried to ping an external IP address (usually the gateway) */
unsigned long lastPing = 0;

/** Current WLAN status */
unsigned int status = WL_IDLE_STATUS;

// =====================================================
void connectWifi() {
  Serial.println("Connecting as wifi client...");
  //WiFi.forceSleepWake();
  WiFi.disconnect();
    WiFi.begin(ssid, password);
  int connRes = WiFi.waitForConnectResult();
  Serial.print("connRes: ");
  Serial.println(connRes);
}
// =====================================================
// Callback for the TaskWIFI  
/**
   check WIFI conditions and try to connect to WIFI.
 * @return void
 */

void WiFi_loop(void){

  // handle a connect request (request is true)
  if (connect) {
    Serial.println("Connect requested");
    connect = false;
    connectWifi();
    lastConnectTry = millis();
  }
  // if not connected try to connect after 60 seconds from last attempt  
  unsigned int s = WiFi.status();
  if (s != WL_CONNECTED && millis() > (lastConnectTry + 60000))
  {
    /* If WLAN disconnected and idle try to connect */
    /* Don't set retry time too low as retry interfere the softAP operation */
    connect = true;
  }
  
  // if connected, try to ping an external IP (usually the gsteway)
  if (s==WL_CONNECTED && millis() > lastPing + 60000){
    _PL("ping 192.168.1.1 for 5 times");
    bool pingok = true;
    for (int i = 0; i < 5; i++)
    {
      if (!Ping.ping("192.168.1.1")){
        pingok = false;
        break;
      }else
      {
        Serial.printf("ping time=%.0f\r\n",Ping.averageTime());
      }
        
    }
    if (pingok){
      _PL("Ping success!");
      int gm = WiFi.getMode();
      if(gm != WIFI_MODE_STA){
        Serial.printf("WiFi status was %d, set it to WIFI_MODE_STA\r\n",gm);
        WiFi.mode(WIFI_MODE_STA);
      }
    }
    else{
      _PL("Ping error :( -> turn on the Access Point");
      WiFi.mode(WIFI_MODE_APSTA);
    }
    lastPing = millis();
  }

  // if WLAN status changed
  if (status != s)
  {
    Serial.printf("Status changed from %d to %d:\r\n",status,s);
    status = s;
    if (s == WL_CONNECTED){
      /* Just connected to WLAN */
      Serial.printf("\r\nConnected to %s\r\n",ssid);
      _PP("IP address ");
      Serial.println(WiFi.localIP());

      // Setup MDNS responder
      /*
          if (!MDNS.begin(myHostname)) {
            Serial.println("Error setting up MDNS responder!");
          } else {
            Serial.println("mDNS responder started");
            // Add service to MDNS-SD
            MDNS.addService("http", "tcp", 80);
          }
          */
      _PL("just connected -> Turn off the Access Point")
      WiFi.mode(WIFI_MODE_STA);
    }
    else if (s == WL_NO_SSID_AVAIL){
      _PL("no SSID available -> turn on the Access Point");
      WiFi.disconnect();
      WiFi.mode(WIFI_MODE_APSTA);
    }
    else{
      _PL("not connected -> turn on the Access Point");
      WiFi.mode(WIFI_MODE_APSTA);

    }
  }

  if (s == WL_CONNECTED){
    //MDNS.update();

  }
  // Do work:
  //DNS
  //dnsServer.processNextRequest();
  //HTTP
  //web_server.handleClient();
  
}
// =====================================================
// tosk run by Taskscheduler to handle WIFI  

class TaskWiFi : public Task {
  public:
    void (*_myCallback)();
    ~TaskWiFi() {};
    TaskWiFi(unsigned long interval, Scheduler* aS, void (* myCallback)() ) :  Task(interval, TASK_FOREVER, aS, true) {
      _myCallback = myCallback;
    };
    bool Callback(){
      _myCallback();
      return true;     
    };
};
Task * myTaskWiFi;

//===================================================
/** Load WLAN credentials from EEPROM */
void loadCredentials() {
  EEPROM.begin(512);
  EEPROM.get(0, ssid); // load ssid
  EEPROM.get(0 + sizeof(ssid), password); // load password
  char ok[2 + 1]; 
  EEPROM.get(0 + sizeof(ssid) + sizeof(password), ok); // load ok. ok means that stored data are valid

 
  EEPROM.end();
  if (String(ok) != String("OK")) {
    ssid[0] = 0;
    password[0] = 0;
    
  }
  Serial.println("Recovered credentials:");
  Serial.println(ssid);
  Serial.println(strlen(password) > 0 ? "********" : "<no password>");
  
}

/** Store WLAN credentials to EEPROM */
void   saveCredentials() {
  EEPROM.begin(512);
  EEPROM.put(0, ssid);
  EEPROM.put(0 + sizeof(ssid), password);
  char ok[2 + 1] = "OK";
  EEPROM.put(0 + sizeof(ssid) + sizeof(password), ok);
  EEPROM.commit();
  EEPROM.end();
}

void AccessPointSetup(){

  softAP_ssid = "ESP32_" + WiFi.macAddress();

  // Access Point Setup
  if(WiFi.getMode() != WIFI_MODE_AP && WiFi.getMode() != WIFI_MODE_APSTA){
      
    WiFi.mode(WIFI_MODE_AP);
    
    /* You can remove the password parameter if you want the AP to be open. */
    WiFi.softAP(softAP_ssid.c_str(), softAP_password.c_str());
    delay(2000); 
    WiFi.softAPConfig(apIP, apIP, netMsk);
    delay(100); // Without delay I've seen the IP address blank
    Serial.println("Access Point set:");
  }else
  {
    Serial.println("Access Point already set:");
  }
  Serial.printf("    SSID: %s\r\n", softAP_ssid.c_str());
  Serial.print("    IP address: ");
  Serial.println(WiFi.softAPIP());
}

void CaptivePortalSetup(){
  AccessPointSetup();
  
  loadCredentials(); // Load WLAN credentials from EEPROM
  connect = strlen(ssid) > 0; // Request WLAN connect if there is a SSID

  _PL("TaskScheduler WIFI Task");
  myTaskWiFi = new TaskWiFi(30,&myScheduler,WiFi_loop);
}