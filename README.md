# MySensors-Efergy-e2
Efergy e2 data decoding and integration with MySensors

The Efergy e2 electricity monitor (http://efergy.com/it/products/electricity-monitors/e2-classic)
provides a sensor that wirelessly sends information about the amount of electricity
you are using to the display monitor. The monitor converts this into kilowatt-hours.

This sketch use a JeeNode v5 (http://jeelabs.net/projects/hardware/wiki/JeeNode) equipped
with 433/868 MHz RFM12B module (www.hoperf.com/upload/rf/RFM12B.pdf) configured in OOK mode
to capture wireless data (on pin D3) and forward it through NRF24L01+ with MySensors library

Created by Gennaro Tortone (gtortone@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.
