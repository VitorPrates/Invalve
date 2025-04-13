#include <WiFi.h>
#include <WebServer.h> //Ponto de acesso ESP
#include <HTTPClient.h> //Responsável por realizar requisições HTTP
#include <DNSServer.h> // Responsável por redirecionar o usuário ao se conectar no esp
//Bibliotecas voltadas para comunicação WEB

#include <ArduinoJson.h>
//Usado para trabalhar com a resposta WEB, que é retornada em JSON

#include <Preferences.h>
//Biblioteca usada para salvar dados na memória flash do aparelho
//Garante que o dispositivo não perca seu identificador


#define pin_valve 27
#define SENSOR_PIN 4
//Conectores de comunicação com os módulos

Preferences preferences;
DNSServer dnsServer;
WebServer server(80);
//Configurações iniciais para comunicação WEB


String ssidInput, passInput;
const byte DNS_PORT = 53;

const char* ssid = "";                 // Nome do WiFi do usuário
const char* password = "";             // Senha do WiFi do usuário
const char* apSSID = "Invalve_Config"; // Nome do Ponto de acesso ESP
const char* apPASS = "12345678";       // Senha do ponto de acesso


String device_name = "abc";
volatile int contagemPulsos = 0;
float fatorCalibracao = 7.5;
unsigned long ultimoTempo = 0;
bool valvula = false;
//Configurações iniciais para uso dos módulos


// Função que exibe o formulário HTML quando o usuário se conecta no ponto de acesso
// Usada na linha 205
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
  //Led pisca para demonstrar inicio de tentativa de conexão

  //Tentando conectar o esp no WIFI
  WiFi.disconnect(true);  // Remove redes salvas previamente
  delay(1000);
  WiFi.mode(WIFI_STA);  // Garante que está no modo Station para permitir novas conexões
  //Inicia uma operação para guardar na memória do dispositivo as credênciais de WIFI do usuário 
  preferences.begin("wifiCreds", false);
  String savedSSID = preferences.getString("ssid", "");//busca o nome do wifi registrado na memória para tentar conexão
  String savedPASS = preferences.getString("pass", "");//busca a senha do wifi registrado na memória para tentar conexão
  //

  Serial.begin(115200); // Inicia o monitor serial, para auxiliar em possíves verificações futuras
  // Em conjunto com a função "Serial.print()" exibe ao desenvolvedor quais operações estão ocorrendo no sistema

  //testar para verificar se pode ser removido
  savedSSID.trim();
  savedPASS.trim();

  // Verifica se há uma rede WIFI e senha registrada na memória
  if (savedSSID != "" && savedPASS != "") {
    //caso sim, inicia um processo de tentar se conectar com a internet 
    Serial.println("Tentando conectar com credenciais salvas...");
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    //166-169 -> Responsável por fazer o LED piscar 2 vezes, indicando tentativa de conexão

    Serial.print("SSID: ");
    Serial.println(savedSSID);
    Serial.print("Senha: ");
    Serial.println(savedPASS);

    WiFi.begin(savedSSID.c_str(), savedPASS.c_str());
    //Função que tenta se conectar no WIFI usando as credênciais registradas na memória

    int tentativas = 0;
    while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(250);
      digitalWrite(LED_BUILTIN, LOW);
      delay(250);
      Serial.print(".");
      tentativas++;
    }
    //180-188 -> tenta 20 vezes realizar a conexão com o WIFI


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
  //192-204 -> Se conectado, prossegue com o resto do sistema, se não, inicia o modo de configuração e registro das credênciais

  // Modo Ponto de acesso (para configurar Wi-Fi)
  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_AP);// Ativa o modo de Ponto de acesso (AP - Access Point) 
  WiFi.softAP(apSSID, apPASS); // Inicia com as credênciais definidas no início do código, Nome da rede = "Invalve_Config", Senha = "12345678"

  //IP do esp para ser acessado via internet para envio de credênciais
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP: ");
  Serial.println(myIP);

  
  dnsServer.start(DNS_PORT, "*", myIP);//Função para redirecionar usuário, quando conectado

  
  // Página inicial (formulário)
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getHTML());
  });

  // Quando chamada, reinicia o sistema
  server.on("/reiniciar", HTTP_GET,[](){
    ESP.restart();
  });

  // Salvando SSID e senha
  server.on("/save", HTTP_POST, []() {
    ssidInput = server.arg("ssid");
    passInput = server.arg("pass");
    //recebendo credenciais

    ssidInput.trim();
    passInput.trim();
    //organizando de forma que o sistema entenda (removendo espaços no início ou final das credênciais recebidas)

    //se nenhuma das credências estiver vazia
    if (ssidInput != "" && passInput != "") {
      //Salva credênciais novas na memória
      preferences.putString("ssid", ssidInput);
      preferences.putString("pass", passInput);

      String device_id = preferences.getString("device_id", "");
      Serial.println(device_id);

      //<REMOVER>
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
    //</REMOVER>
  });

  // Redireciona qualquer rota para "/"
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  }); 
  // $ 221-271 -> funções responsáveis por devolver ao usuário páginas HTML para lhe auxiliar

  server.begin();//iniciar o servidor do dispositivo

  // Loop de espera no modo AP, garante que o server irá receber e enviar informações
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

//função responsável por verificar as tarefas que o sistema deve executar
String tarefas() {
    //verifica se está conectado
    if (WiFi.status() == WL_CONNECTED) {
        //Se conectado, inicia o processo de verificação do banco de dados, em busca de uma tarefa 
        HTTPClient http;
        String url = "https://fatec-aap-vi-backend.onrender.com/api/queues?device=" + device_name; 
        http.begin(url);
        
        int httpCode = http.GET();

        if (httpCode > 0) {  // Sucesso
            //Caso tenha achado uma tarefa, começa o processo de entender qual tarefa é
            String payload = http.getString();//devolutiva do banco de dados
            
            // Processar o JSON, conjunto de informações devolvidas pelo Banco de dados
            const size_t capacidadeBuffer = 1024;
            StaticJsonDocument<capacidadeBuffer> doc;
            DeserializationError erro = deserializeJson(doc, payload);

            //Caso não consiga entender a tarefa, a função retorna um erro 
            if (erro) {
                Serial.print("Erro ao parsear JSON: ");
                Serial.println(erro.c_str());
                return "erro";
            }

            // Caso tenha entendido a função, coleta apenas o que é útil ao sistema, e devolve o que tem que ser feito
            // Acessar valores do JSON
            String message = doc["message"];
            String command = doc["data"]["event"]["command"];
            
            return command; //retorna o comando a ser realizado pelo sistema
        } else {
            //caso de erro em comunicação com o banco de dados
            Serial.print("Erro na requisição: ");
            Serial.println(httpCode);
        }
        http.end();//Fim da verificação de tarefa
    } else {
        //caso de erro em comunicação com o WIFI
        Serial.println("WiFi desconectado!");
        ESP.restart();
    }
}

//Ocorre quando o usuário solicita informações do fluxo de vazão que o sistema está lendo
void enviarinfos(String device, float media_fluxo)
{
   //verifica se o sistema está conectado
  if (WiFi.status() == WL_CONNECTED) {
        //Se conectado, começa o processo de envio ao banco de dados a informação solicitada 
        HTTPClient http;
        http.begin("https://fatec-aap-vi-backend.onrender.com/api/queues/water_flow");
        http.addHeader("Content-Type", "application/json");

        // Criando o JSON exigido pelo banco de dados
        StaticJsonDocument<200> doc;
        doc["device"] = device;
        doc["average_water_flow"] = media_fluxo;

        //Organiza os dados para serem interpretados no banco de dados
        String jsonStr;
        serializeJson(doc, jsonStr);

        // Enviando a requisição POST - Envia via internet a infomação solicitada
        int httpResponseCode = http.POST(jsonStr);
        
        // Pegando a resposta do banco de dados
        String resposta = http.getString();

        Serial.print("Código HTTP: ");
        Serial.println(httpResponseCode);
        Serial.print("Resposta: ");
        Serial.println(resposta);

        http.end();
    } else {
        //Caso não conectado, não realiza a função, mas não impede o funcionamento do sistema
        Serial.println("WiFi desconectado!");
    }
}

//Inicia todo o sistema, ao ligar o aparelho
void setup() {
  //Define as conexões físicas dos módulos
  pinMode(pin_valve, OUTPUT);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  //Pisca o led para demonstrar que o sistema está iniciando

  inicializarWiFi();
  //Chama a função para que o sistema se conecte no WIFI

  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), contarPulso, RISING);
  //Inicia a configuralão do leitor de fluxo de vazão

  Serial.print("Conectando");

  digitalWrite(LED_BUILTIN, HIGH);
  //Mantém o LED azul ligado para demonstrar que o sistema está totalmente configurado e pronto para uso
}

String task = "No event";
void loop() {
  task = tarefas();
  //Chama a função de tarefas e armazena a tarefa a ser executada

  unsigned long tempoAgora = millis();
  // A cada 1 segundo verificar o fluxo de vazão
  // Utiliza pulsos devido a forma como o sensor envia seus dados, em forma de pulsos

  float vazaoLPorMinuto = (contagemPulsos / fatorCalibracao);
  //Calcula a vazão de acordo com a calibragem previamente definida e fixa do sistema

  //garante que a verificação do fluxo de vazão ocorra a cada 1 segundo
  if (tempoAgora - ultimoTempo >= 1000) {  
    detachInterrupt(SENSOR_PIN); 
    
    Serial.print("Vazão: ");
    Serial.print(vazaoLPorMinuto);
    Serial.println(" L/min");
  
    contagemPulsos = 0;
    ultimoTempo = tempoAgora;
  
    attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), contarPulso, RISING);
    //Garante que sistema e o módulo estejam alinhados quanto ao seu tempo de funcionamento, em conjunto com a função "detachInterrupt"
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
  // $ 439 - 475 -> Realiza ações de acordo com a tarefa retornada, e pisca o LED para demonstrar que a tarefa foi realizada

  //verifica se a variável "valvula" está como false ou true, fechada ou aberta 
  if(valvula)
  {
    digitalWrite(pin_valve, HIGH);
  }
  else
  {
    digitalWrite(pin_valve, LOW);
  }
}
