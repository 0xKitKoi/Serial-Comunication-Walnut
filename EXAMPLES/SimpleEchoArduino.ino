// SimpleEchoProgram.ino

void setup() {
  Serial.begin(9600);
}

void loop() {
  if (Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      Serial.println("got: " + line);
    }
  }
}