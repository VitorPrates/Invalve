#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define pin_valve 27
#define SENSOR_PIN 4

const char* ssid = "VIVOFIBRA-6C40";      // Nome do WiFi
const char* password = "A647ED4AA0"; // Senha do WiFi

String device_name = "abc";

volatile int contagemPulsos = 0;
float fatorCalibracao = 7.5;
unsigned long ultimoTempo = 0;

bool valvula = false;

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
  // WiFi.begin(ssid, password);
  Serial.begin(115200);

  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), contarPulso, RISING);


  Serial.print("Conectando");
  
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(1000);
  //   Serial.print("...");
  // }
  // Serial.println("Conectado");

  // tarefas();
}

// String task = "No event";
void loop() {
  // task = tarefas();
  
  // Serial.println(task);
  
  // if(task == "close")
  // {
  //   valvula = true;
  // }
  // else if(task == "open")
  // {
  //   valvula = false;
  // }
  // else if(task == "check")
  // {
  //   // Serial.println("teste:" + String(fluxo_vazao) + "/valvula=" + String(valvula));
  //   // enviarinfos(device_name, fluxo_vazao);
  // }
  // if(valvula)
  // {
  //   digitalWrite(pin_valve, HIGH);
  // }
  // else
  // {
  //   digitalWrite(pin_valve, LOW);
  // }
  unsigned long tempoAgora = millis();
  
  // A cada 1 segundo verificar o fluxo de vazão
  // Utiliza pulsos devido a forma como o sensor envia seus dados, em formar de pulsos
  if (tempoAgora - ultimoTempo >= 1000) {  
    detachInterrupt(SENSOR_PIN); 
  
    float vazaoLPorMinuto = (contagemPulsos / fatorCalibracao);
    Serial.print("Vazão: ");
    Serial.print(vazaoLPorMinuto);
    Serial.println(" L/min");
  
    contagemPulsos = 0;
    ultimoTempo = tempoAgora;
  
    attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), contarPulso, RISING);
  }
}
