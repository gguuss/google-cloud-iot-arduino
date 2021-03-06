/******************************************************************************
 * Copyright 2018 Google
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <jwt.h>
#include <time.h>

// Wifi newtork details.
const char* ssid = "SSID";
const char* password = "PASSWORD";

// Cloud iot details.
const char* project_id = "project-id";
const char* location = "us-central1";
const char* registry_id = "my-registry";
const char* device_id = "my-python-device";
// To get the private key run (where private-key.pem is the ec private key
// used to create the certificate uploaded to google cloud iot):
// openssl ec -in <private-key.pem> -noout -text
// and copy priv: part.
const char* private_key_str =
    "5a:2e:06:b5:c1:f2:9c:b3:77:b2:89:f5:29:29:93:"
    "07:fd:ed:22:0d:03:2b:a6:b1:b6:04:0b:d5:9b:49:"
    "7d:ca";

// TODO: Use root certificate to verify tls connection rather than using a
// fingerprint.
// To get the fingerprint run
// openssl s_client -connect cloudiotdevice.googleapis.com:443 -cipher <cipher>
// Copy the certificate (all lines between and including ---BEGIN CERTIFICATE---
// and --END CERTIFICATE--) to a.cert. Then to get the fingerprint run
// openssl x509 -noout -fingerprint -sha1 -inform pem -in a.cert
// <cipher> is probably ECDHE-RSA-AES128-GCM-SHA256, but if that doesn't work
// try it with other ciphers obtained by sslscan cloudiotdevice.googleapis.com.
const char* fingerprint =
    "67 BB 57 B0 9A A7 BA AE 53 13 6E 73 E7 88 D9 1D 0C D3 8F 7F";

unsigned int priv_key[8];

std::string getJwt() {
  // Disable software watchdog as these operations can take a while.
  ESP.wdtDisable();
  std::string jwt = CreateJwt(project_id, time(nullptr), priv_key);
  ESP.wdtEnable(0);
  return jwt;
}

const char* host = "cloudiotdevice.googleapis.com";
const int httpsPort = 443;

WiFiClientSecure* client;
std::string pwd;

// Fills the priv_key global variable with private key str which is of the form
// aa:bb:cc:dd:ee:...
void fill_priv_key(const char* priv_key_str) {
  priv_key[8] = 0;
  for (int i = 7; i >= 0; i--) {
    priv_key[i] = 0;
    for (int byte_num = 0; byte_num < 4; byte_num++) {
      priv_key[i] = (priv_key[i] << 8) + strtoul(priv_key_str, NULL, 16);
      priv_key_str += 3;
    }
  }
}

// Gets the google cloud iot http endpoint path.
std::string get_path(const char* project_id, const char* location,
                     const char* registry_id, const char* device_id) {
  return std::string("/v1/projects/") + project_id + "/locations/" + location +
         "/registries/" + registry_id + "/devices/" + device_id;
}

void setup() {
  fill_priv_key(private_key_str);

  // put your setup code here, to run once:
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting on time sync...");
  while (time(nullptr) < 1510644967) {
    delay(10);
  }

  client = new WiFiClientSecure;
  Serial.println("Connecting to mqtt.googleapis.com");
  client->connect(host, httpsPort);
  Serial.println("Verifying certificate");
  if (!client->verify(fingerprint, host)) {
    Serial.println(
        "Error: Certificate not verified! "
        "Perhaps the fingerprint is outdated.");
    // return;
  }

  Serial.println("Getting jwt.");
  pwd = getJwt();
  Serial.println(pwd.c_str());

  // Connect via https.
  std::string handshake =
      "GET " + get_path(project_id, location, registry_id, device_id) +
      "/config?local_version=1 HTTP/1.1\n"
      "Host: cloudiotdevice.googleapis.com\n"
      "cache-control: no-cache\n"
      "authorization: Bearer " +
      pwd +
      "\n"
      "\n";
  Serial.println("Sending: '");
  Serial.print(handshake.c_str());
  Serial.println("'");
  client->write((const uint8_t*)handshake.c_str(), (int)handshake.size());
  client->flush();

  int tries = 500;
  while (!client->available() && (tries-- > 0)) delay(10);

  if (client->available()) {
    char rdBuf[1000];
    int bread = client->read((uint8_t*)rdBuf, 1000);
    Serial.println("Response: ");
    Serial.write(rdBuf, bread);
  } else {
    Serial.println("No response, something went wrong.");
  }
}

void loop() {}
