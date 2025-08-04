version 2.0 incluye

uso de THINGSBOARD (dasboard)
uso de DISCORD     (alerta de mensaje)
uso de BEACON (bluetooth)     (llave de propietario)

COMPONETES:

●	ESP32 como cerebro
●	Sensor de Movimiento (PIR):
●	Módulo Sensor de Luz (LDR):
●	Potenciómetro (Control del Umbral de Luz):
●	Botón de Reset:
●	Módulo de Relé (Doble Canal):
●	Buzzer Activo:
●	LED Rojo
●	RESISTENCIA 200 ohm

CONEXIONES

◆	Conecta el pin VIN del ESP32 a la línea roja (+) de un lado de la protoboard.
◆	Conecta un pin GND del ESP32 a la línea azul (-) de ese mismo lado.
◆	Conecta el pin 3V3 del ESP32 a la línea roja (+) del otro lado de la protoboard.
◆	Conecta el otro pin GND del ESP32 a la línea azul (-) de ese lado.

●	Sensor de Movimiento (PIR):
○	VCC -> al riel de 5V.
○	GND -> al riel de GND (-).
○	OUT -> al pin D27 del ESP32.

●	Módulo Sensor de Luz (LDR):
○	VCC -> al riel de 5V.
○	GND -> al riel de GND (-).
○	OUT -> al pin D27 del ESP32.

●	Potenciómetro
○	Pin central -> al pin D35 del ESP32.
○	Un pin del extremo -> al riel de 3.3V.
○	El otro pin del extremo -> al riel de GND (-).

●	Botón de Reset:
○	Un pin -> al pin D25 del ESP32.
○	El pin de al lado (en el lado opuesto del mecanismo) -> al riel de GND (-).

●	Módulo de Relé 
○	Pin VCC -> al riel de 5V.
○	Pin GND -> al riel de GND (-).
○	Pin IN1 -> al pin D26 del ESP32.

●	Buzzer Activo:
○	Pata negativa (-) -> al riel de GND (-).
○	Pata positiva (+) -> al tornillo NO (Normalmente Abierto) del relé (el del grupo K1).

●	Puente del Relé:
○	Un cable desde el riel de 5V al tornillo COM (el del medio) del relé (grupo K1).

●	LED 
○	Pata corta (-) del LED -> al riel de GND (-).
○	Pata larga (+) del LED -> a una pata de la resistencia (220Ω o 330Ω).
○	La otra pata de la resistencia -> al pin D23 del ESP32.






