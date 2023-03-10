# Bewässerungsanlage - Steuerung per Wemos D1 Mini (MQTT) 
Dieser Sketch dient zur Ansteuerung einer Ralaisplatine (Kuman Relay Shield Module 8 Kanäle Relais Modul Brett 5V).<br>
Er wurde mit der Arduino IDE erstellt.<br>
Folgendes Board wurde dabe gewählt: Werkzeuge >> Bord >> ESP8266 Boards (2.5.0) >> Wemos D1 R1.

Die Relais schalten:
- ein Hauptschütz, das wiederum eine Tauchpumpe schaltet
- bis zu sieben elektromagnetische Ventile für die verschiedenen Gießkreise 

Die Kommunikation erfolgt über das MQTT-Protokoll.<br>
Es können einzelne Relais direkt angesteuert werden. Dabei wird sichergestellt, dass max. 2 Relais gleichzeitig eingeschaltet sind, um eine Überlastung des Wemos D1 Mini und der Relaisplatine zu verhindern.<br>

Ein zusätzliches Programm (Prog1) ermöglicht den Start einzelner Relais (Ventile bzw. Gießkreise) in einer Abfolge nacheinander mit individueller Einschaltdauer. Die Einschaltdauer der einzelnen Relais wird als Payload übergeben, z.B. 8|8|5, d.h. Relais 1 und 2 ist je 8 Minuten und Relais 3 für 5 Minuten eingeschaltet.

Ein weiteres Programm (Prog2) ermöglicht den Start einzelner Relais (Ventile bzw. Gießkreise) in einer Abfolge nacheinander. Die Einschaltdauer für jedes Relais ist gleich und wird als Payload übergeben, z.B. 10, d.h. jedes Relais ist für 10 Minuten eingeschaltet.

Weiterführende Informationen sind auf https://smarthome.kaempf-nk.de/bewaesserungsanlage/programmierung-wemos-d1-mini.html zu finden.
