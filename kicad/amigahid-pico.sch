EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L Connector_Generic:Conn_01x08 J1
U 1 1 61D08C53
P 6100 1150
F 0 "J1" V 6064 662 50  0000 R CNN
F 1 "Amiga keyboard input" V 5973 662 50  0000 R CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical" H 6100 1150 50  0001 C CNN
F 3 "~" H 6100 1150 50  0001 C CNN
	1    6100 1150
	0    -1   -1   0   
$EndComp
Text Notes 6450 1000 1    50   ~ 0
1 CLK\n2 DAT\n3 RES\n4 +5V\n5 n/c\n6 GND\n7 PWR\n8 DRV
Wire Wire Line
	6100 1350 6100 1550
Wire Wire Line
	4950 1550 4950 1800
Wire Wire Line
	6300 1350 6300 1550
Wire Wire Line
	6300 1550 7800 1550
Wire Wire Line
	7800 1550 7800 2100
Wire Wire Line
	4950 2400 4950 2500
Wire Wire Line
	4950 2500 5100 2500
$Comp
L MCU_RaspberryPi_and_Boards:Pico U2
U 1 1 61D29AF8
P 6200 3650
F 0 "U2" V 6154 4728 50  0000 L CNN
F 1 "Pico" V 6245 4728 50  0000 L CNN
F 2 "MCU_RaspberryPi_and_Boards:RPi_Pico_SMD_TH" V 6200 3650 50  0001 C CNN
F 3 "" H 6200 3650 50  0001 C CNN
	1    6200 3650
	0    1    1    0   
$EndComp
Wire Wire Line
	5800 1700 5800 1350
Wire Wire Line
	5900 1350 5900 1700
Wire Wire Line
	6000 1700 6000 1350
Wire Wire Line
	5950 2950 5950 2800
Wire Wire Line
	5950 2800 7800 2800
Wire Wire Line
	7800 2800 7800 2100
Connection ~ 7800 2100
Wire Wire Line
	4650 2100 4600 2100
Wire Wire Line
	4600 2100 4600 4600
Wire Wire Line
	4600 4600 6950 4600
Wire Wire Line
	7800 4600 7800 2800
Connection ~ 7800 2800
Wire Wire Line
	4950 1550 4400 1550
Wire Wire Line
	4400 1550 4400 4550
Wire Wire Line
	4400 4550 7150 4550
Wire Wire Line
	7150 4550 7150 4350
Connection ~ 4950 1550
Wire Wire Line
	6950 4350 6950 4600
Connection ~ 6950 4600
Wire Wire Line
	6950 4600 7800 4600
$Comp
L Device:C_Small C1
U 1 1 61D3DC4E
P 5300 2000
F 0 "C1" V 5071 2000 50  0000 C CNN
F 1 "C 0.1μF" V 5162 2000 50  0000 C CNN
F 2 "Capacitor_SMD:C_0603_1608Metric_Pad1.08x0.95mm_HandSolder" H 5300 2000 50  0001 C CNN
F 3 "~" H 5300 2000 50  0001 C CNN
	1    5300 2000
	0    1    1    0   
$EndComp
Wire Wire Line
	4950 1550 5100 1550
$Comp
L Regulator_Linear:LM1117-3.3 U3
U 1 1 61D06DF8
P 4950 2100
F 0 "U3" V 4904 2205 50  0000 L CNN
F 1 "LM1117-3.3" V 4995 2205 50  0000 L CNN
F 2 "Package_TO_SOT_SMD:SOT-223-3_TabPin2" H 4950 2100 50  0001 C CNN
F 3 "http://www.ti.com/lit/ds/symlink/lm1117.pdf" H 4950 2100 50  0001 C CNN
	1    4950 2100
	0    1    1    0   
$EndComp
Wire Wire Line
	7800 2100 6800 2100
$Comp
L Logic_LevelTranslator:TXS0108EPW U1
U 1 1 61D46D89
P 6100 2100
F 0 "U1" V 6146 1356 50  0000 R CNN
F 1 "TXS0108EPW" V 6055 1356 50  0000 R CNN
F 2 "Package_SO:TSSOP-20_4.4x6.5mm_P0.65mm" H 6100 1350 50  0001 C CNN
F 3 "www.ti.com/lit/ds/symlink/txs0108e.pdf" H 6100 2000 50  0001 C CNN
	1    6100 2100
	0    -1   -1   0   
$EndComp
Wire Wire Line
	5100 1550 5100 2000
Wire Wire Line
	5100 2000 5200 2000
Connection ~ 5100 1550
Wire Wire Line
	5100 1550 6100 1550
$Comp
L Device:C_Small C2
U 1 1 61D49E53
P 5300 2200
F 0 "C2" V 5071 2200 50  0000 C CNN
F 1 "C 0.1μF" V 5162 2200 50  0000 C CNN
F 2 "Capacitor_SMD:C_0603_1608Metric_Pad1.08x0.95mm_HandSolder" H 5300 2200 50  0001 C CNN
F 3 "~" H 5300 2200 50  0001 C CNN
	1    5300 2200
	0    1    1    0   
$EndComp
Wire Wire Line
	5200 2200 5100 2200
Wire Wire Line
	5100 2200 5100 2500
Connection ~ 5100 2500
Wire Wire Line
	5100 2500 5700 2500
Wire Wire Line
	5800 2500 5800 2550
Wire Wire Line
	5800 2550 5650 2550
Wire Wire Line
	5650 2550 5650 2950
Wire Wire Line
	5900 2500 5900 2600
Wire Wire Line
	5900 2600 5750 2600
Wire Wire Line
	5750 2600 5750 2950
Wire Wire Line
	6000 2500 6000 2650
Wire Wire Line
	6000 2650 5850 2650
Wire Wire Line
	5850 2650 5850 2950
NoConn ~ 6200 1350
NoConn ~ 6400 1350
NoConn ~ 6500 1350
NoConn ~ 6100 1700
NoConn ~ 6200 1700
NoConn ~ 6300 1700
NoConn ~ 6400 1700
NoConn ~ 6500 1700
NoConn ~ 6500 2500
NoConn ~ 6400 2500
NoConn ~ 6300 2500
NoConn ~ 6200 2500
NoConn ~ 6100 2500
NoConn ~ 5250 2950
NoConn ~ 5350 2950
NoConn ~ 5450 2950
NoConn ~ 5550 2950
NoConn ~ 6050 2950
NoConn ~ 6150 2950
NoConn ~ 6250 2950
NoConn ~ 6350 2950
NoConn ~ 6450 2950
NoConn ~ 6550 2950
NoConn ~ 6650 2950
NoConn ~ 6750 2950
NoConn ~ 6850 2950
NoConn ~ 6950 2950
NoConn ~ 7050 2950
NoConn ~ 7150 2950
NoConn ~ 5050 3550
NoConn ~ 5050 3650
NoConn ~ 5050 3750
NoConn ~ 7050 4350
NoConn ~ 5250 4350
NoConn ~ 5350 4350
NoConn ~ 5450 4350
NoConn ~ 5550 4350
NoConn ~ 5650 4350
NoConn ~ 5750 4350
NoConn ~ 5850 4350
NoConn ~ 5950 4350
NoConn ~ 6050 4350
NoConn ~ 6250 4350
NoConn ~ 6350 4350
NoConn ~ 6150 4350
NoConn ~ 6450 4350
NoConn ~ 6550 4350
NoConn ~ 6650 4350
NoConn ~ 6750 4350
NoConn ~ 6850 4350
Text Notes 7400 7500 0    50   ~ 0
amiga keyboard adapter board
Text Notes 8150 7650 0    50   ~ 0
2022-01-01
Text Notes 10600 7650 0    50   ~ 0
1
$EndSCHEMATC
