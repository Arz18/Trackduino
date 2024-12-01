//Autores: Ariel A. Arzuaga y Gilberto Ramos

#include <SoftwareSerial.h>

#define led_azul 10 //para la función blink
#define led_verde 9 //para sms
#define led_rojo 8  //para errores de gnss

SoftwareSerial sim7000(2, 3); // RX, TX para el módulo SIM7000G

const int powerPin = 4;  // Pin de control ON/OFF para el módulo
String gnssData = "";    // Variable global para almacenar datos GNSS
String latitude = "";    // Variable para almacenar la última latitud
String longitude = "";   // Variable para almacenar la última longitud
unsigned long previousMillis = 0; // Para controlar el tiempo de lectura del GNSS
const long interval = 5000;       // Intervalo para leer datos GNSS

void setup() {

  pinMode(led_azul, OUTPUT);  // Led de debug y gnss
  pinMode(led_verde, OUTPUT); //Led SMS
  pinMode(led_rojo, OUTPUT); //Led gnss no coordenadas
  pinMode(powerPin, OUTPUT);

  Serial.begin(9600);  // Comunicación con el monitor serie a 9600 baudios
  sim7000.begin(9600); // Comunicación con el SIM7000G a 9600 baudios

  // Encender el módulo SIM7000G
  digitalWrite(powerPin, HIGH);
  delay(3000);  // Mantiene HIGH por 3 segundos para encender el módulo
  digitalWrite(powerPin, LOW);
  delay(3000); 

  // Inicio de comunicación
  Serial.println("Iniciando prueba de SIM7000G...");

  // Establecer baudrate del módulo de 115,200 (default) a 9600
  sim7000.println("AT+IPR=9600"); // Comando para establecer baudios
  delay(1000);
  printResponse();
  digitalWrite(led_azul, HIGH); // Luz azul para saber por donde va el boot sequence. 

  // Guardar la configuración
  sim7000.println("AT&W");
  delay(1000);
  printResponse();

  // Configurar el modo de texto para SMS
  sim7000.println("AT+CMGF=1");
  delay(3000);
  printResponse();
  
  // Configurar el almacenamiento de SMS en la memoria interna del módulo SIM7000G
  sim7000.println("AT+CPMS=\"ME\",\"ME\",\"ME\"");
  delay(3000);
  printResponse();
  digitalWrite(led_verde, HIGH);

  // Limpiar mensajes anteriores
  sim7000.println("AT+CMGD=1,4");  // Borrar todos los mensajes de la memoria
  delay(5000);
  printResponse();

  // Encender el módulo GNSS
  sim7000.println("AT+CGNSPWR=1");  // Encender el GNSS
  delay(5000);
  printResponse();
  digitalWrite(led_rojo, HIGH);

  // Configurar el módulo para notificar automáticamente los SMS entrantes
  sim7000.println("AT+CNMI=2,2,0,0,0"); 
  delay(3000);
  printResponse();

  sim7000.println("AT+CSQ"); // FUERZA DE SENAL LTE, debe ser menor de 99.99
  delay(500);

  // Deshabilitar el eco de comandos, esto evita llenar el buffer
  sim7000.println("ATE0");
  delay(500);
  printResponse();
  //boot sequence leds off
  digitalWrite(led_azul, LOW);
  digitalWrite(led_verde, LOW);
  digitalWrite(led_rojo, LOW);

}

void loop() {
  // Verificar si hay un SMS entrante para que encienda el LED en el Arduino
  if (sim7000.available() > 0) {
    digitalWrite(led_verde, HIGH);
    String smsContent = sim7000.readString();  // Leer todo el contenido del SMS
    Serial.print("SMS recibido: ");
    Serial.println(smsContent);
    digitalWrite(led_verde, LOW);

    // Verificar si el SMS contiene la palabra "marco" o "Marco"
    if (smsContent.indexOf("marco") != -1 || smsContent.indexOf("Marco") != -1) {
      
      Serial.println("Solicitud de ubicación recibida, enviando mensaje...");
      sendLocationSMS();
    }
  }

  // Leer datos GNSS usando un intervalo de tiempo
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    sim7000.println("AT+CGNSINF");  // Solicitar información GNSS
    
    delay(500); 
    gnssData = "";

    // Leer datos GNSS en una cadena
    while (sim7000.available() > 0) {
      char c = sim7000.read();
      gnssData += c;
    }

    // Imprimir los datos GNSS en el monitor serie para verificar
    Serial.println("Datos GNSS:");
    Serial.println(gnssData);
    
    // Procesar los datos GNSS
    parseGNSSData(gnssData);
  }
}

// Función para analizar los datos GNSS (parsing)
void parseGNSSData(String data) {
  int index = data.indexOf("+CGNSINF:");  // Encontrar el inicio de la respuesta
  if (index != -1) {
    String gnssInfo = data.substring(index + 9);  // Extraer los datos después de "+CGNSINF:"
    gnssInfo.trim();  // Eliminar espacios en blanco al inicio y al final

    // Dividir la cadena en campos separados por comas
    int numFields = 0;
    String fields[22];  // Hay hasta 21 campos en la respuesta

    while (gnssInfo.length() > 0 && numFields < 22) {
      int commaIndex = gnssInfo.indexOf(',');
      if (commaIndex != -1) {
        fields[numFields++] = gnssInfo.substring(0, commaIndex);
        gnssInfo = gnssInfo.substring(commaIndex + 1);
      } else {
        fields[numFields++] = gnssInfo;
        break;
      }
    }

    // Verificar que tenemos al menos 5 campos para latitud y longitud
    if (numFields >= 5) {
      String runStatus = fields[0];
      String fixStatus = fields[1]; // Fix del GPS
      String utcDateTime = fields[2];
      latitude = fields[3];  // Campo 4 es la latitud
      longitude = fields[4]; // Campo 5 es la longitud
      //altitude = fields[5]; //altura no se usa

      // Mostrar los valores en el Serial Monitor
      Serial.println("Latitud almacenada: " + latitude);
      Serial.println("Longitud almacenada: " + longitude);
      
      // Verificar el fixStatus
  if (fixStatus == "1") {
    // Hay fijación GNSS válida
    blink(1);  // Parpadea el LED azul (PPS)
  } else {
    Serial.println("Advertencia: No hay fijación GNSS válida.");
    digitalWrite(led_rojo, HIGH);
    delay(300);
    digitalWrite(led_rojo, LOW);
  }

    } else {
      Serial.println("Error: Datos GNSS incompletos.");
    }
  } else {
    Serial.println("Error: No se encontró '+CGNSINF:' en los datos GNSS.");
  }
}

/////////////////////////////// FUNCIONES NO BORRAR ///////////////////////////////////////////////

// Función para enviar un SMS con la ubicación actual
void sendLocationSMS() {
  // Verificar que las coordenadas estén disponibles
  if (latitude.length() > 0 && longitude.length() > 0) { // SI TIENE COORDENADAS
    // String mensaje = "Ubicacion actual: Latitud: " + latitude + ", Longitud: " + longitude;
    String mensaje1 = "Polo!\nhttps://maps.google.com/?q=" + latitude + "," + longitude; // Formato Google Maps
    sim7000.println("AT+CMGF=1");  // Modo de texto para SMS
    delay(3000);
    printResponse();

    sim7000.println("AT+CMGS=\"+17875167622\"");  // Cambia a tu número
    delay(5000);  // Aumentar el tiempo de espera para asegurar que el módulo esté listo
    printResponse();

    sim7000.print(mensaje1);
    sim7000.write(26);  // Código de fin de SMS (Ctrl+Z)
    delay(7000);
    printResponse();
  } else { // SI NO TIENE COORDENADAS
    String mensaje2 = "Hmm...No se donde estoy... Posiblemente no hay visibilidad de los satelites, intenta mas tarde.";
    sim7000.println("AT+CMGF=1");  // Modo de texto para SMS
    delay(3000);
    printResponse();

    sim7000.println("AT+CMGS=\"+17875167622\"");  // Numero de telefono de la persona que recibira los mensajes 
    delay(5000);  // Aumentar el tiempo de espera para asegurar que el módulo esté listo
    printResponse();

    sim7000.print(mensaje2);
    sim7000.write(26);  // Código de fin de SMS (Ctrl+Z)
    delay(7000);
    printResponse();
  }
}

// Función para imprimir la respuesta del módulo SIM7000G
void printResponse() {
  unsigned long startTime = millis();
  while (millis() - startTime < 5000) { // Espera hasta 5 segundos por una respuesta
    while (sim7000.available()) {
      char c = sim7000.read();
      Serial.write(c);
      startTime = millis(); // Reiniciar tiempo de espera al recibir datos
    }
  }
}

// Función de debug con LED y para gnss
void blink(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(led_azul, HIGH);
    delay(100);
    digitalWrite(led_azul, LOW);
    delay(100);
  }
}
