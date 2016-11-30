/*
ARDUINO MONITORAMENTO SITE B
Zabbix Agente (ativo) para enviar para o servidor Zabbix as informações abaixo:
* Leitura do sensor de temperatura/umidade SHT;
* Leitura do sensor de abertura/fechamento da porta do Data Center.
* Leitura do sensor barômetro (pressão atmosférica e altitude).
* Sensor de fumaça.
O programa envia os dados coletados dos sensores para o Host configurano no Zabbix
utilizando Zabbix trapper.

Host Zabbix: TIO_ARDUINO_SITE_B
Itens Zabbix: 1 = temperatura, 2 = umidade, 3 = pressao, 4 = altitude, 5= fumaca, 6 = porta;

Limites pressão atmosférica:
1002 hPa = ATENÇÂO
1001 hPa = ALERTA

Limites Umidade:
30% à 85% = OK
umi < 30% | umi > 85% = ATENÇÂO

Limites Temperatura:
15 à 30 = OK
temp < 15 = ALERTA Temperatura abaixo do normal, verificar.
temp > 30 = CRITICA Temperatura alta verificar.

Criado: 24 Novembro 2016
Última modificação: 30 Novembro 2016
Autor: Jardel F. F. de Araujo
Versão: 1.0

*/

//-----------------INCLUDES / DEFINES--------------------
#include <SPI.h>
#include <Ethernet.h>
#include <DHT.h>
#include <SHT1x.h>
#include <Adafruit_BMP085.h>
#include <Wire.h>
#include <string.h>

#define DHTPIN 8               // Pino DHT com resistor 10k ou 4,7k
#define DHTTYPE DHT11          // Tipo sensor DHT (DHT11/DHT22)
#define LED_PIN                // LED sensor presença
#define LED2_PIN               // LED envio notificação Zabbix
#define PIR_PIN                // Pino sensor presença
#define RELE_PIN               // Pino de sinal do Relê
#define SENSORCH A0            // Pino analogico do senso de alagamento
#define PORTAPIN 5             // Pino do sensor de porta
#define SENSORFUMACA A1        // Pino do sensor de fumaça
#define SHTDATA 7              // Pino do DATA do sensor de temperatura/umidade SHT
#define SHTCLOCK  6            // Pino do CLOCK do sensor de tempe
//--------------------------------------------------------

//-----------------VARIÁVEIS GLOBAIS--------------------
const char host[] = "TIO_ARDUINO_SITE_B";                                                       // Host Zabbix
const char* keys[] = { "temperatura", "umidade", "pressao", "altitude", "fumaca", "porta" };    // Itens monitorados Zabbix
const String token = "Token de 25 caracteres";                                                  // Token de identificacao do Arduino para envio das notificacoes
const char* zabbixSenderHost[] = { "XXX.XXX.XXX.XXX", "Host: XXX.XXX.XXX.XXX" };                // Endereco host do servidor da API do Zabbix Sender

String temperatura, umidade, pressao, vAltitude, vAgua, fumacaPPM;								// Variáveis de armazenamento dos valores dos sensores
char dados[80];
float valor;
byte ind, tempo;
int Porta = 1;        // Estado da porta (1 = fechada, 0 = aberta)
int vFumaca = 0;      // Variável para armazenar estado do sensor de fumaça
int VSensor;	      // Variável para armazenamento temporário do valor do sensor de alagamento
byte checkPorta = 0;  // Status da checagem da porta (0 = esta fecha, 1 = esta aberta);
byte checkFumaca = 0; // Status da checagem do sensor fumaça (0 = esta ok, 1 = detectado fumaça);
//------------------------------------------------------

//-----------------DEFINIÇÕES DA REDE--------------------
byte mac[] = { 0x4E, 0x61, 0x67, 0x69, 0x6F, 0x73 };              // MAC Address Shield de Ethernet
IPAddress ip(192, 168, 166, 35);                                  // IP Arduino
IPAddress gateway(192, 168, 166, 1);                              // IP Gateway
IPAddress subnet(255, 255, 255, 0);                               // Mascara de rede
IPAddress zabbix(189, 45, 192, 189);                              // IP do servidor/proxy Zabbix
//-------------------------------------------------------

//-----------------INSTANCIAMENTO DE OBJETOS--------------------
Adafruit_BMP085 bmp;                                              // Objeto do sensor do barômetro         
DHT dht(DHTPIN, DHTTYPE);                                         // Objeto do sensor de temperatura/umidade DHT
SHT1x sht1x(SHTDATA, SHTCLOCK);                                   // Objeto do sensor de temperatura/umidade SHT
//-------------------------------=====--------------------------

void setup()
{
	Serial.begin(9600);
	//pinMode(LED_PIN, OUTPUT);
	//pinMode(LED2_PIN, OUTPUT);
	//pinMode(RELE_PIN, OUTPUT);
	//pinMode(LED3_PIN, OUTPUT);
	//pinMode(PIR_PIN, INPUT);
	//digitalWrite(LED3_PIN, LOW);
	//digitalWrite(RELE_PIN, LOW);
	Ethernet.begin(mac, ip, gateway, subnet);
	Serial.print("IP Arduino: ");
	Serial.println(Ethernet.localIP());
	if (!bmp.begin()) {
		Serial.println("Não foi possível localizar o sensor BMP085 (barometro), verifique as conexões!");
		sendzabbix("-999", 3);
		sendzabbix("-999", 4);
	}
	pinMode(PORTAPIN, INPUT_PULLUP);	// Sensor magnetico da porta (abertura/fechamento) no pin 5
	sendzabbix("0", 6);					// Notifica inicialmente que a porta está fechada.
	tempo = 0;
}

// Loop principal
void loop()
{
	sensor_porta();
	sensor_fumaca();
	// Se o tempo for maoir ou igual á 30 (corresponde à 60 segundos) faz a checagem dos sensores abaixo
	if (tempo >= 30) {
		temperaruta_sht();
		umidade_sht();
		sensor_barometro();
		altitude();
		//sensor_alagamento();
		tempo = 0;
	}
	// Aguarda 2 segundos até um novo ciclo do loop
	delay(2000);
	tempo++;
}

// Faz a leitura do sensor da porta e envia notificacao com os dados para o Nagios
void sensor_porta() {
	Porta = digitalRead(PORTAPIN); // Lê valor do sensor da porta
	if (Porta != 1) {
		if ((checkPorta != 1) || (tempo == 30)) {
			sendzabbix("1", 6);
			Serial.println("Porta aberta.");
			checkPorta = 1;
		}
	}
	else {
		if ((checkPorta != 0) || (tempo == 30)) {
			sendzabbix("0", 6);
			Serial.println("Porta fechada.");
			checkPorta = 0;
		}
	}
}

// Faz a leitura da pressao atmosférica do sensor barometro e envia notificacao com os dados para o Nagios
void sensor_barometro() {
	valor = bmp.readPressure() / 100;
	if ((valor < 1002) && ((valor >= 1001))) {
		ind = 1;
	}
	else if (valor < 1001) {
		ind = 2;
	}
	else {
		ind = 0;
	}
	pressao = "Pressão atmosférica: " + String(valor);
	pressao = pressao + " hPa";
	Serial.println(pressao);
	pressao.toCharArray(dados, 80);
	sendzabbix(String(valor), 3);
}

// Faz a leitura da altitude do sensor barometro e envia notificacao com os dados para o Nagios
// Obs.: Valor aproximado devido variacao da pressao atmosferica.
void altitude() {
	String valAlt = String(bmp.readAltitude());
	vAltitude = "Altidude: ≅ " + valAlt;
	vAltitude = vAltitude + " m";
	Serial.println(vAltitude);
	sendzabbix(valAlt, 4);
}

// Faz a leitura do sensor de chuva/alagamento e envia notificacao com os dados para o Nagios
void sensor_alagamento() {
	VSensor = analogRead(SENSORCH);
	if (VSensor >= 1000) {
		vAgua = "Sensor alagamento AC OK: Está seco (" + String(VSensor) + ")";
		Serial.println(vAgua);
		vAgua.toCharArray(dados, 80);
		//nagduino.sendNRDP(HOST, SRV_ALAGAMENTO, 0, dados);
	}
	else {
		vAgua = "Sensor alagamento AC CRITICAL: Detectado água! (" + String(VSensor) + ")";
		Serial.println(vAgua);
		vAgua.toCharArray(dados, 80);
		//nagduino.sendNRDP(HOST, SRV_ALAGAMENTO, 2, dados);
	}
}

// Faz a leitura do sensor de fumaça e envia notificacao com os dados para o Nagios
void sensor_fumaca() {
	vFumaca = analogRead(SENSORFUMACA);
	if (vFumaca >= 525) {
		if ((checkFumaca != 1) || (tempo == 30)) {
			fumacaPPM = "*TESTE* Detectado fumaça! Valor analogico/PPM: " + String(vFumaca) + "/" + String(map(vFumaca, 0, 1023, 0, 2500));
			fumacaPPM.toCharArray(dados, 80);
			sendzabbix(String(vFumaca), 5);
			Serial.println("Sensor fumaça: Detectado fumaça! Valor analogico: " + String(vFumaca));
			checkFumaca = 1;
		}
	}
	else {
		if ((checkFumaca != 0) || (tempo == 30)) {
			fumacaPPM = "Sensor fumaça OK! Valor analogico/PPM: " + String(vFumaca) + "/" + String(map(vFumaca, 0, 1023, 0, 2500));
			fumacaPPM.toCharArray(dados, 80);
			sendzabbix(String(vFumaca), 5);
			Serial.println("Sensor fumaça: OK! Valor analogico: " + String(vFumaca));
			checkFumaca = 0;
		}
	}
}

// Funcao para fazer a leitura do sensor de umidade (Sensor SHT10) e envia notificacao com os dados para o Nagios
void umidade_sht() {
	// Se nao conseguir fazer a leitura do sensor retorna UNKNOW
	if (sht1x.readHumidity() < 0.0) {
		sendzabbix("-999", 2);
	}
	else {
		sendzabbix(String(sht1x.readHumidity()), 2);
		umidade = "Umidade: " + String((float)sht1x.readHumidity());
		umidade = umidade + "%";
		Serial.println(umidade);
	}
}

//
// Funcao para fazer a leitura do sensor de umidade (Sensor DHT11) e envia notificacao com os dados para o Nagios
//

void umidade_dht() {
	// Se nao conseguir fazer a leitura do sensor retorna UNKNOW
	if (isnan(dht.readHumidity())) {
		sendzabbix("-999", 2);
	}
	else {
		sendzabbix(String(dht.readHumidity()), 2);
		umidade="Umidade: " + String((float)dht.readHumidity());
		umidade=umidade + "%";
		Serial.println(umidade);
	}
}


//
// Funcao para fazer a leitura do sensor de umidade (Sensor SHT10) e envia notificacao com os dados para o Nagios
//
void temperaruta_sht() {
	// Se nao conseguir fazer a leitura do sensor retorna UNKNOW
	if (sht1x.readTemperatureC() < -40.0) {
		sendzabbix("-999", 1);
	}
	else {
		sendzabbix(String(sht1x.readTemperatureC()), 1);
		temperatura = "Temperatura: " + String((float)sht1x.readTemperatureC());
		temperatura = temperatura + " °C";
		Serial.println(temperatura);
	}
}

//
// Funcao para fazer a leitura do sensor de temperatura (Sensor DHT11) e envia notificacao com os dados para o Nagios
//

void temperatura_dht() {
	if (isnan(dht.readTemperature())) {
	sendzabbix("-999", 1);
	}
	else {
		sendzabbix(String(dht.readTemperature()), 1);
		temperatura = "Temperatura: " + String(dht.readTemperature());
		temperatura=temperatura + " *C";
		Serial.println(temperatura);
	}
}


// Cliente ZabbixAgent que envia os dados dos sensores para o servidor do Zabbix.
void* sendzabbix(String valor, int sensor) {
	EthernetClient zabclient;
	String data, key;
	byte rip[] = { 0,0,0,0 };
	key = String(keys[sensor - 1]);
	data = "host=" + String(host) + "&key=" + key + "&value=" + valor + "&token=" + token;
	if (zabclient.connect(zabbixSenderHost[0], 80)) { // ZABBIX SERVER ADDRESS
		zabclient.getRemoteIP(rip);
		Serial.print(F("Conectado no servidor Zabbix: "));
		for (int bcount = 0; bcount < 4; bcount++)
		{
			Serial.print(rip[bcount], DEC);
			if (bcount<3) Serial.print(".");
		}
		Serial.println(F(""));
		Serial.println("Conectado com o servidor Zabbix, <key> " + key + " enviando valor: " + valor);
		zabclient.println("POST /zbxagent/zbxagent.py HTTP/1.1");
		zabclient.println(zabbixSenderHost[1]); // SERVER ADDRESS HERE TOO
		zabclient.println("Content-Type: application/x-www-form-urlencoded; charset=UTF-8");
		zabclient.print("Content-Length: ");
		zabclient.println(data.length());
		zabclient.println();
		zabclient.print(data);
	}
	if (zabclient.connected()) {
		zabclient.stop();  // DISCONNECT FROM THE SERVER
	}
}
