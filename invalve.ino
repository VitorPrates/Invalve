#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>

#define pin_valve 27
#define SENSOR_PIN 4

Preferences preferences;
DNSServer dnsServer;
WebServer server(80);

String ssidInput, passInput;
const byte DNS_PORT = 53;

const char* ssid = "";      // Nome do WiFi
const char* password = "";           // Senha do WiFi

const char* apSSID = "Invalve_Config";
const char* apPASS = "12345678";  // mínimo 8 caracteres


String device_name = "abc";

volatile int contagemPulsos = 0;

float fatorCalibracao = 7.5;
unsigned long ultimoTempo = 0;

bool valvula = false;

// Função que exibe o formulário HTML
String getHTML() {
  return R"rawliteral(
    <!DOCTYPE html>
    <html>
    <body>
      <h2>Configuração Wi-Fi Invalve</h2>
      <form action="/save" method="POST">
        Sua internet: <input type="text" name="ssid"><br><br>
        Senha: <input type="text" name="pass"><br><br>
        <input type="submit" value="Salvar">
      </form>
    </body>
    </html>
  )rawliteral";
}

String buscar_id(String ssid, String senha) {
  WiFi.begin(ssid, senha);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado ao Wi-Fi.");
    Serial.println("Buscando um ID...");

    HTTPClient http;
    String url = "http://fatec-aap-vi-backend.onrender.com/api/devices"; 
    http.begin(url);

    // Prepara o JSON de envio
    StaticJsonDocument<200> docSend;
    docSend["mac"] = WiFi.macAddress();  // Você pode trocar isso se quiser usar outro identificador
    docSend["tipo"] = "ESP32";

    String payloadEnvio;
    serializeJson(docSend, payloadEnvio);

    // Cabeçalho da requisição
    http.addHeader("Content-Type", "application/json");

    // Envia POST
    int httpCode = http.POST(payloadEnvio);

    // Verifica resposta
    if (httpCode > 0) {
      String payload = http.getString();

      Serial.print("HTTP Code: ");
      Serial.println(httpCode);

      Serial.println("Resposta da API:");
      Serial.println(payload);

      // Parse do JSON
      StaticJsonDocument<1024> doc;
      DeserializationError erro = deserializeJson(doc, payload);

      if (erro) {
        Serial.print("Erro ao parsear JSON: ");
        Serial.println(erro.c_str());
        return "erro";
      }

      // Extrai o token
      if (doc.containsKey("data") && doc["data"].containsKey("token")) {
        String token = doc["data"]["token"].as<String>();
        return token;
      } else {
        Serial.println("Resposta JSON não contém 'data.token'");
        return "erro";
      }
    } else {
      Serial.print("Erro na requisição HTTP: ");
      Serial.println(httpCode);
      return "erro";
    }

    http.end();
  } else {
    Serial.println("Falha ao conectar no Wi-Fi.");
    return "erro";
  }
}

// Função principal de inicialização do Wi-Fi
void inicializarWiFi() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);

  WiFi.disconnect(true);  // Remove redes salvas
  delay(1000);
  WiFi.mode(WIFI_STA);    // Garante que está no modo Station
  Serial.begin(115200);

  preferences.begin("wifiCreds", false);
  String savedSSID = preferences.getString("ssid", "");
  String savedPASS = preferences.getString("pass", "");

  savedSSID.trim();
  savedPASS.trim();

  if (savedSSID != "" && savedPASS != "") {
    Serial.println("Tentando conectar com credenciais salvas...");
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    Serial.print("SSID: ");
    Serial.println(savedSSID);
    Serial.print("Senha: ");
    Serial.println(savedPASS);
    WiFi.begin(savedSSID.c_str(), savedPASS.c_str());
    int tentativas = 0;
    while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(250);
      digitalWrite(LED_BUILTIN, LOW);
      delay(250);
      Serial.print(".");
      tentativas++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConectado com sucesso!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      preferences.end();
      digitalWrite(LED_BUILTIN, HIGH);
      return;
    } else {
      Serial.println("\nFalha ao conectar. Entrando em modo AP...");
    }
  } else {
    Serial.println("Credenciais não encontradas. Entrando em modo AP...");
  }

  // Modo Access Point (para configurar Wi-Fi)
  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPASS);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP: ");
  Serial.println(myIP);

  dnsServer.start(DNS_PORT, "*", myIP);

  // Página inicial (formulário)
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getHTML());
  });

  server.on("/reiniciar", HTTP_GET,[](){
    ESP.restart();
  });

  // Salvando SSID e senha
  server.on("/save", HTTP_POST, []() {
    ssidInput = server.arg("ssid");
    passInput = server.arg("pass");

    ssidInput.trim();
    passInput.trim();

    if (ssidInput != "" && passInput != "") {
      preferences.putString("ssid", ssidInput);
      preferences.putString("pass", passInput);
      String device_id = preferences.getString("device_id", "");
      Serial.println(device_id);
      if(device_id == "" || device_id == "erro")
      {
        device_id = buscar_id(ssidInput, passInput);
        if(device_id != "erro")
        {
          server.send(200, "text/html", "<h1>Credenciais salvas - [ERRO ID] - Reiniciando...</h1> <form action='/reiniciar' method='GET'> <input type='submit' value='Continuar'></form>");
        }
        else
        {
          preferences.putString("device_id", device_id);
          server.send(200, "text/html", "<h1>Credenciais salvas - ID do dispositivo:"+ device_id +" - Reiniciando...</h1> <form action='/reiniciar' method='GET'> <input type='submit' value='Continuar'></form>");
        }
      }
      else
      {
        server.send(200, "text/html", "<h1>Credenciais salvas - ID do dispositivo:"+ device_id +" - Reiniciando...</h1> <form action='/reiniciar' method='GET'> <input type='submit' value='Continuar'></form>");
      }
      delay(2000);
    } else {
      server.send(200, "text/html", "<h1>SSID e Senha não podem estar vazios!</h1>");
    }
  });

  // Redireciona qualquer rota para "/"
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();

  // Loop de espera no modo AP
  while (true) {
    dnsServer.processNextRequest();
    server.handleClient();
    digitalWrite(LED_BUILTIN, HIGH);
    delay(30);
    digitalWrite(LED_BUILTIN, LOW);
    delay(30);
  }
}

void IRAM_ATTR contarPulso() {
  contagemPulsos++;
}

String tarefas() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = "https://fatec-aap-vi-backend.onrender.com/api/queues?device=" + device_name; 
        http.begin(url);
        
        int httpCode = http.GET();
        if (httpCode > 0) {  // Sucesso
            String payload = http.getString();
            // Serial.println("Resposta JSON:");
            // Serial.println(payload);
            
            // Processar o JSON
            const size_t capacidadeBuffer = 1024;
            StaticJsonDocument<capacidadeBuffer> doc;
            DeserializationError erro = deserializeJson(doc, payload);

            if (erro) {
                Serial.print("Erro ao parsear JSON: ");
                Serial.println(erro.c_str());
                return "erro";
            }

            // Acessar valores do JSON
            String message = doc["message"];
            String command = doc["data"]["event"]["command"];
            
            // Serial.print("message: ");
            // Serial.println(message);
            // return message != "null" ? command : message;
            return command;
        } else {
            Serial.print("Erro na requisição: ");
            Serial.println(httpCode);
        }
        
        http.end();
    } else {
        Serial.println("WiFi desconectado!");
        ESP.restart();
    }
}

void enviarinfos(String device, float media_fluxo)
{
  if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin("https://fatec-aap-vi-backend.onrender.com/api/queues/water_flow");
        http.addHeader("Content-Type", "application/json");

        // Criando o JSON
        StaticJsonDocument<200> doc;
        doc["device"] = device;
        doc["average_water_flow"] = media_fluxo;

        String jsonStr;
        serializeJson(doc, jsonStr);

        // Enviando a requisição POST
        int httpResponseCode = http.POST(jsonStr);
        
        // Pegando a resposta do servidor
        String resposta = http.getString();

        Serial.print("Código HTTP: ");
        Serial.println(httpResponseCode);
        Serial.print("Resposta: ");
        Serial.println(resposta);

        http.end();
    } else {
        Serial.println("WiFi desconectado!");
    }
}

void setup() {
  pinMode(pin_valve, OUTPUT);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);

  inicializarWiFi();
  
  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), contarPulso, RISING);

  Serial.print("Conectando");
  
  digitalWrite(LED_BUILTIN, HIGH);
}

String task = "No event";
void loop() {
  task = tarefas();

  unsigned long tempoAgora = millis();
  // A cada 1 segundo verificar o fluxo de vazão
  // Utiliza pulsos devido a forma como o sensor envia seus dados, em formar de pulsos
  float vazaoLPorMinuto = (contagemPulsos / fatorCalibracao);
  if (tempoAgora - ultimoTempo >= 1000) {  
    detachInterrupt(SENSOR_PIN); 
    
    Serial.print("Vazão: ");
    Serial.print(vazaoLPorMinuto);
    Serial.println(" L/min");
  
    contagemPulsos = 0;
    ultimoTempo = tempoAgora;
  
    attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), contarPulso, RISING);
  }
  
  Serial.println(task);
  
  if(task == "close")
  {
    valvula = true;
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
  }
  else if(task == "open")
  {
    valvula = false;
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
  }
  else if(task == "check")
  {
    // Serial.println("teste:" + String(fluxo_vazao) + "/valvula=" + String(valvula));
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    enviarinfos(device_name, vazaoLPorMinuto);
  }
  if(valvula)
  {
    digitalWrite(pin_valve, HIGH);
  }
  else
  {
    digitalWrite(pin_valve, LOW);
  }
}
