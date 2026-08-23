#define INPUT_PIN 2
#define OUTPUT_PIN 3
#define SIGNAL_PIN 5

#define millis_t unsigned long

// Only use every n-th input trigger, completely ignore all in-between.
uint8_t n_th_frame = 2;

// Minimum time between two output trigger events.
uint16_t min_time_between_triggers = 70;

// Time of the next scheduled lowering of the output pin
millis_t lower_tgt = 0;

// True if the output pin is scheduled to be lowered
bool todo_lower = false;

// true if SIGNAL_PIN is scheduled to be lowered
bool todo_lower_signal = false;

// Time of the next scheduled lowering of the SIGNAL_PIN
millis_t lower_signal_tgt = 0;

// Time of the last detected rising edge
millis_t last_input_rising = 0;

// Total number of registered input events
uint32_t input_trigger_count = 0;

uint32_t output_trigger_count = 0;

// Time of the previous output trigger event
millis_t previous_output_trigger = 0;

uint32_t raw_interrupt_count = 0;
bool raw_interrupt_incremented = false;

void interrupt_rising(millis_t time)
{
  todo_lower_signal = true;
  digitalWrite(SIGNAL_PIN, HIGH);
  lower_signal_tgt = time + 20;
  raw_interrupt_count++;
  raw_interrupt_incremented = true;
  // Debounce, if the last rising flank is not old enough, completely ignore everything.
  if (time < last_input_rising + (min_time_between_triggers / 2)) {
    return;
  }
  last_input_rising = time;
  input_trigger_count++;
  if (0 != (input_trigger_count % n_th_frame)) {
    return;
  }
  if (time < previous_output_trigger + min_time_between_triggers) {
    return;
  }
  previous_output_trigger = time;
  digitalWrite(OUTPUT_PIN, HIGH);
  todo_lower = true;
  lower_tgt = time + 20;
  output_trigger_count++;
}

millis_t last_change = 0;

void interrupt()
{
  millis_t time = millis();
  if (time < last_change + 3) {
    return;
  }
  uint8_t input_state = digitalRead(INPUT_PIN);
  last_change = time;
  digitalWrite(SIGNAL_PIN, input_state);
  if (HIGH == input_state) {
    interrupt_rising(time);
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(OUTPUT_PIN, OUTPUT);
  pinMode(SIGNAL_PIN, OUTPUT);

  pinMode(INPUT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INPUT_PIN), interrupt, CHANGE);
}

void loop()
{
  millis_t time = millis();
  if (time > previous_output_trigger + 500 && time > last_input_rising + 500) {
    digitalWrite(OUTPUT_PIN, HIGH);
    previous_output_trigger = time;
    todo_lower = true;
    lower_tgt = time + 20;
  }
  if (todo_lower) {
    if (time > lower_tgt) {
      digitalWrite(OUTPUT_PIN, LOW);
      todo_lower = false;
    }
  }
  if (todo_lower_signal) {
    if (time > lower_signal_tgt) {
      digitalWrite(SIGNAL_PIN, LOW);
      todo_lower_signal = false;
    }
  }
  if (raw_interrupt_incremented) {
    noInterrupts();
    Serial.print("Debounced interrupts ");
    Serial.print(raw_interrupt_count);
    Serial.print(" registered ");
    Serial.print(input_trigger_count);
    Serial.print(" output ");
    Serial.println(output_trigger_count);
    raw_interrupt_incremented = false;
    interrupts();
  }

#if 0
  while (Serial.available()) {
    Serial.read();
    digitalWrite(OUTPUT_PIN, HIGH);
    delay(300);
    digitalWrite(OUTPUT_PIN, LOW);
    delay(300);
  }
#endif
}
