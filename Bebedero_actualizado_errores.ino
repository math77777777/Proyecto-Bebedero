#include <Arduino_FreeRTOS.h>
#include <semphr.h>

// Pines
#define PIN_BOYA 2
#define PIN_VALVULA 4
#define PIN_BOMBA 3
#define LED 7

// Variables de control
bool vaciadoActivo = false;

// Semáforo de exclusión para la válvula
SemaphoreHandle_t mutexValvula;

void setup() {
  pinMode(PIN_BOYA, INPUT_PULLUP);
  pinMode(PIN_VALVULA, OUTPUT);
  pinMode(PIN_BOMBA, OUTPUT);
  pinMode(LED, OUTPUT);

  digitalWrite(PIN_VALVULA, HIGH);  // Válvula cerrada (relé apagado)
  digitalWrite(PIN_BOMBA, HIGH);    // Bomba apagada

  Serial.begin(9600);

  mutexValvula = xSemaphoreCreateMutex();

  xTaskCreate(tareaValvula, "Valvula", 128, NULL, 1, NULL);
  xTaskCreate(tareaBomba, "Bomba", 128, NULL, 1, NULL);
}

void loop() {
  // Nada en loop
}

// --------------------
// TAREA DE LA VÁLVULA
// --------------------
// Variables para detectar fallo de boya
TickType_t tiempoInicioLowBoya = 0;
bool sistemaEnFallo = false;

void tareaValvula(void* pvParameters) {
  while (1) {
    if (!vaciadoActivo) {
      bool boyaBaja = digitalRead(PIN_BOYA) == LOW;

      xSemaphoreTake(mutexValvula, portMAX_DELAY);

      if (sistemaEnFallo) {
        // Si ya está en fallo, mantenemos la válvula cerrada
        digitalWrite(PIN_VALVULA, HIGH);
        xSemaphoreGive(mutexValvula);
        vTaskDelay(pdMS_TO_TICKS(1000));
        continue;
      }

      if (boyaBaja) {
        TickType_t ahora = xTaskGetTickCount();
        if (tiempoInicioLowBoya == 0) {
          tiempoInicioLowBoya = ahora;
        } else if ((ahora - tiempoInicioLowBoya) > pdMS_TO_TICKS(30000)) {
          sistemaEnFallo = true;
          digitalWrite(LED, HIGH);
          Serial.println("ERROR1: Boya en nivel bajo por más de 10 segundos");
          Serial.println("   -> Causa posible 1: Presión de agua baja");
          Serial.println("   -> Causa posible 2: Válvula obstruida");
          digitalWrite(PIN_VALVULA, HIGH); // cerrar válvula por seguridad
          xSemaphoreGive(mutexValvula);
          vTaskDelay(pdMS_TO_TICKS(1000));
          continue;
        }
        digitalWrite(PIN_VALVULA, LOW);  // Abrir válvula
        Serial.println("Válvula ABIERTA (llenando)");
      } else {
        digitalWrite(PIN_VALVULA, HIGH); // Cerrar válvula
        Serial.println("Válvula CERRADA (lleno)");
        tiempoInicioLowBoya = 0; // Reset del contador de error
      }

      xSemaphoreGive(mutexValvula);
    }

    vTaskDelay(pdMS_TO_TICKS(1000)); // Revisar cada 1 segundo
  }
}


// --------------------
// TAREA DE LA BOMBA
// --------------------
void tareaBomba(void* pvParameters) {
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(15000));  // Esperar 24 horas (86400000 ms)

    vaciadoActivo = true;
    Serial.println("INICIO VACIADO AUTOMÁTICO");

    xSemaphoreTake(mutexValvula, portMAX_DELAY);
    digitalWrite(PIN_VALVULA, HIGH);     // Cerrar válvula antes del vaciado
    xSemaphoreGive(mutexValvula);

    digitalWrite(PIN_BOMBA, LOW);        // Encender bomba
    Serial.println("Bomba ENCENDIDA");

    vTaskDelay(pdMS_TO_TICKS(20000));    // Mantener por 20 segundos

    digitalWrite(PIN_BOMBA, HIGH);       // Apagar bomba
    Serial.println("Bomba APAGADA");

    // 🔍 Verificar boya después del vaciado
    if (digitalRead(PIN_BOYA) == HIGH) {
      sistemaEnFallo = true;
      digitalWrite(LED, HIGH);
      Serial.println("ERROR2: Boya indica lleno justo después del vaciado");
      Serial.println("   -> Causa posible: Boya desconectada o dañada");
    }

    vaciadoActivo = false;
    Serial.println("FIN DEL VACIADO");
  }
}
