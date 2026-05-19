# Architecture Block Diagrams

> Mermaid diagrams render natively in Obsidian (no plugin needed). Right-click any diagram → "Open in new tab" để xem fullscreen.
>
> **Related**: [[../PROJECT-SUMMARY|Project Summary]] · [[../firmware/architecture|Firmware Architecture]] · [[../HDSD-Lap-Rap-Robot|HDSD Master Guide]]

---

## 1. System-level block diagram

Hardware overview — robot + dock + cloud:

```mermaid
graph TB
    subgraph Robot["🤖 ROBOT (ESP32-S3-CAM N16R8)"]
        MCU[ESP32-S3<br/>16MB flash<br/>8MB PSRAM]
        CAM[OV3660<br/>DVP camera]
        PTZ[2x MG90S<br/>Pan + Tilt]
        DRV[L298N H-bridge<br/>+ 2x BO motor]
        IMU[MPU6050 IMU]
        US[4x HC-SR04<br/>F/B/L/R]
        AUD[INMP441 mic<br/>MAX98357A amp<br/>3W speaker]
        SIM[SIM800L GSM]
        OLED[SSD1306 OLED]
        IRR[TSOP38238 IR rx]
        SC[Spring contacts]
        BAT[3S 18650<br/>+ BMS]
    end

    subgraph Dock["🔌 DOCK STATION"]
        CP[Copper plates<br/>50x30mm]
        IRT[IR LED 940nm<br/>38kHz modulated]
        AC[AC-DC 12.6V/3A]
    end

    subgraph Cloud["☁️ CLOUD / LAN"]
        WiFi[WiFi router 2.4GHz]
        HASS[Home Assistant<br/>+ Mosquitto]
        TG[Telegram Bot]
    end

    subgraph Family["👨‍👩‍👧 FAMILY"]
        Phone[Smartphone]
    end

    MCU --- CAM
    MCU --- PTZ
    MCU --- DRV
    MCU --- IMU
    MCU --- US
    MCU --- AUD
    MCU --- SIM
    MCU --- OLED
    MCU --- IRR

    SC -.touch.-> CP
    IRR -.scan.-> IRT
    AC --> CP
    CP -.charge.-> BAT

    MCU <-- WiFi STA --> WiFi
    WiFi <--> HASS
    HASS --> TG
    TG --> Phone
    SIM ==SMS+call==> Phone

    classDef hw fill:#2a4d6e,color:#fff,stroke:#4488cc
    classDef dock fill:#6e4d2a,color:#fff,stroke:#cc8844
    classDef cloud fill:#2a6e4d,color:#fff,stroke:#44cc88
    class MCU,CAM,PTZ,DRV,IMU,US,AUD,SIM,OLED,IRR,SC,BAT hw
    class CP,IRT,AC dock
    class WiFi,HASS,TG,Phone cloud
```

---

## 2. Power tree (4 rails từ battery)

```mermaid
graph TB
    BAT[3x 18650 Battery<br/>11.1V nominal<br/>9-12.6V range]
    BAT --> FUSE[Fuse PTC 5A]
    FUSE --> R12{+12V rail<br/>8A max}

    R12 --> MOT[L298N VS<br/>motor power<br/>~4A peak]
    R12 --> B1[LM2596S #1<br/>buck → 5V/3A]
    R12 --> B2[LM2596S #2<br/>buck → 4V/2A<br/>SEPARATE!]

    B1 --> R5{+5V rail<br/>3A max}
    R5 --> ESP[ESP32-S3-CAM<br/>module VIN]
    R5 --> SRV[2x servo<br/>peak 1.2A]
    R5 --> AMP[MAX98357A]
    R5 --> US5[4x HC-SR04]
    R5 --> LDO[AP2112K LDO]

    B2 --> R4{+4V rail<br/>2A peak}
    R4 --> SIM[SIM800L only<br/>2A burst TX]

    LDO --> R3{+3.3V rail<br/>600mA}
    R3 --> IMU[MPU6050]
    R3 --> OLED[SSD1306]
    R3 --> MIC[INMP441]
    R3 --> PU[Pull-ups]

    DOCK[Dock 12.6V/3A] -.dock.-> BMS[3S BMS<br/>+ balance]
    BMS --> BAT

    classDef bat fill:#cc4444,color:#fff
    classDef rail fill:#4488cc,color:#fff
    classDef load fill:#888888,color:#fff
    classDef warn fill:#cc8844,color:#fff
    class BAT bat
    class R12,R5,R4,R3 rail
    class B2,R4 warn
```

> **Critical**: rail 4V tách riêng buck #2 (không cascade từ 5V). Lý do: SIM800L burst 2A trong 577µs sẽ kéo sụt rail 5V → ESP32 reset.

---

## 3. Firmware task graph

9 FreeRTOS task + 10 driver, data flow:

```mermaid
graph TB
    subgraph L3["🧠 LAYER 3 — Behavior"]
        BHV[task_behavior<br/>IDLE/PATROL/RTH/DOCKED/SOS]
    end

    subgraph L2["⚙️ LAYER 2 — Tasks"]
        SF[task_sensor_fusion<br/>20Hz]
        NAV[task_navigation<br/>50Hz watchdog]
        PTZ[task_ptz<br/>50Hz smooth]
        DK[task_dock<br/>FSM 10Hz]
        AUD[task_audio<br/>async]
        SOS[task_sos<br/>event-driven]
        OL[task_oled<br/>2Hz]
        MQ[task_mqtt<br/>1Hz publish]
        SCH[task_schedule<br/>cron]
    end

    subgraph L1["🔧 LAYER 1 — Drivers"]
        D_IMU[mpu6050]
        D_US[hcsr04]
        D_MOT[motor_l298n]
        D_SRV[servo_pwm]
        D_AUD[audio_i2s]
        D_SIM[sim800l]
        D_BAT[battery]
        D_IR[ir_dock]
        D_OL[ssd1306]
        D_I2C[i2c_bus<br/>shared 400kHz]
    end

    SF --> D_IMU
    SF --> D_US
    NAV --> D_MOT
    PTZ --> D_SRV
    AUD --> D_AUD
    SOS --> D_SIM
    DK --> D_BAT
    DK --> D_IR
    DK --> NAV
    OL --> D_OL
    OL --> D_BAT
    D_IMU --> D_I2C
    D_OL --> D_I2C

    BHV --> NAV
    BHV --> DK
    BHV --> PTZ

    SF -.EventGroup.-> BHV
    SF -.EventGroup.-> SOS
    SF -.Queue overwrite.-> NAV
    SF -.Queue overwrite.-> OL
    SF -.Queue overwrite.-> MQ

    SCH -.timer.-> BHV
    MQ -.subscribe.-> BHV

    classDef bhv fill:#cc4488,color:#fff
    classDef task fill:#4488cc,color:#fff
    classDef driver fill:#888888,color:#fff
    class BHV bhv
    class SF,NAV,PTZ,DK,AUD,SOS,OL,MQ,SCH task
    class D_IMU,D_US,D_MOT,D_SRV,D_AUD,D_SIM,D_BAT,D_IR,D_OL,D_I2C driver
```

---

## 4. Behavior FSM (top-level)

```mermaid
stateDiagram-v2
    [*] --> IDLE

    IDLE --> PATROL: /behavior/patrol
    IDLE --> RETURN_HOME: battery_low()
    PATROL --> RETURN_HOME: battery_low()
    PATROL --> IDLE: /behavior/idle
    RETURN_HOME --> DOCKED: dock state CHARGING
    RETURN_HOME --> FAULT: dock FAULT
    DOCKED --> IDLE: /behavior/leave\nor battery_full()

    state SOS_ACTIVE {
        [*] --> Frozen
        Frozen --> Wait: motor brake\ntask_sos dispatch
        Wait --> [*]: 30s elapsed\n&& EVT_FALL_CLEAR
    }

    IDLE --> SOS_ACTIVE: EVT_FALL_DETECTED
    PATROL --> SOS_ACTIVE: EVT_FALL_DETECTED
    RETURN_HOME --> SOS_ACTIVE: EVT_FALL_DETECTED
    DOCKED --> SOS_ACTIVE: EVT_FALL_DETECTED

    SOS_ACTIVE --> IDLE: resume previous
    FAULT --> IDLE: /behavior/idle
```

---

## 5. Dock FSM (task_dock)

```mermaid
stateDiagram-v2
    [*] --> DK_IDLE

    DK_IDLE --> DK_SEARCH: /dock/start
    DK_SEARCH --> DK_APPROACH: beacon strength > 60
    DK_SEARCH --> DK_FAULT: 30s timeout
    DK_APPROACH --> DK_CONTACT: front < 30cm
    DK_APPROACH --> DK_SEARCH: beacon lost
    DK_APPROACH --> DK_FAULT: 60s timeout
    DK_CONTACT --> DK_CHARGING: dock_voltage > 10V
    DK_CONTACT --> DK_FAULT: 30s timeout
    DK_CHARGING --> DK_CHARGED: VBAT > 12.4V steady 5min
    DK_CHARGING --> DK_CONTACT: lost charge
    DK_CHARGED --> DK_IDLE: /dock/leave\n(drive fwd 1.5s)
    DK_FAULT --> DK_IDLE: /dock/cancel

    DK_IDLE: IDLE
    DK_SEARCH: SEARCH\n(rotate CCW @30dps)
    DK_APPROACH: APPROACH\n(forward @30%)
    DK_CONTACT: CONTACT\n(creep @15%)
    DK_CHARGING: CHARGING\n(motor off)
    DK_CHARGED: CHARGED\n(idle on dock)
    DK_FAULT: FAULT
```

---

## 6. Fall → SOS sequence (event flow)

```mermaid
sequenceDiagram
    autonumber
    actor U as 👴 User
    participant IMU as MPU6050
    participant SF as task_sensor_fusion
    participant EG as EventGroup
    participant SOS as task_sos
    participant SIM as SIM800L
    participant BHV as task_behavior
    participant NAV as task_navigation
    participant FAM as 📱 Family

    U->>IMU: Falls (accel > 2.5g)
    loop 50Hz polling
        IMU->>SF: read accel/gyro burst
    end
    SF->>SF: state SPIKE → wait 500ms
    SF->>SF: check tilt > 60°
    Note over SF: confirmed FALL
    SF->>EG: set EVT_FALL_DETECTED

    par SOS dispatch
        EG-->>SOS: bit set, wake up
        SOS->>SIM: AT+CMGS=phone1
        SIM->>FAM: 📨 SMS delivered
        SOS->>SIM: ATD phone1;
        SIM->>FAM: 📞 ring 30s
    and Behavior preempt
        EG-->>BHV: bit set
        BHV->>NAV: emergency_stop()
        NAV->>NAV: motor brake
        BHV->>BHV: enter SOS_ACTIVE
    end

    Note over U,BHV: 🛑 Robot frozen, family alerted

    U->>U: stands robot up
    SF->>SF: tilt < 30° for 10s
    SF->>EG: set EVT_FALL_CLEAR
    EG-->>BHV: clear bit
    BHV->>BHV: resume previous state (PATROL/IDLE)
```

---

## 7. MQTT integration topology

```mermaid
graph LR
    subgraph Robot
        TM[task_mqtt<br/>publisher 1Hz]
        BHV[task_behavior]
    end

    subgraph HASS["Home Assistant"]
        MOSQ[Mosquitto broker]
        ENT[MQTT entities<br/>11x sensor/button]
        AUTO[Automations]
        TG[Telegram notify]
    end

    subgraph Phone
        FAM[👨‍👩‍👧 Family chat]
    end

    TM -->|elderly_robot/state<br/>JSON 1Hz| MOSQ
    TM -->|elderly_robot/event/fall| MOSQ
    TM -->|elderly_robot/event/battery| MOSQ
    TM -->|elderly_robot/event/dock| MOSQ

    MOSQ -->|elderly_robot/cmd/behavior| BHV

    MOSQ --> ENT
    ENT --> AUTO
    AUTO --> TG
    TG -->|"⚠️ Fall alert"| FAM
    TG -->|"🔋 Battery low"| FAM

    classDef sub fill:#2a4d6e,color:#fff
    classDef cloud fill:#2a6e4d,color:#fff
    class TM,BHV sub
    class MOSQ,ENT,AUTO,TG cloud
```

---

## 8. Daily schedule timeline

```mermaid
gantt
    title Robot daily routine (NVS-configurable)
    dateFormat HH:mm
    axisFormat %H:%M

    section Morning
    Leave dock      :a1, 06:00, 30m
    Patrol          :a2, after a1, 4h

    section Lunch
    Return home     :b1, 12:00, 30m
    Charge          :b2, after b1, 2h
    Leave dock      :b3, 14:00, 30m

    section Evening
    Patrol evening  :c1, 19:00, 3h
    Return home     :d1, 22:00, 30m
    Night charge    :d2, after d1, 8h
```

> Adjust qua `GET /schedule/set?i=0&h=7&m=30&cmd=leave` để fit nhịp sinh hoạt người dùng.

---

## 9. LEDC peripheral allocation

```mermaid
graph TB
    subgraph LEDC["ESP32-S3 LEDC peripheral"]
        T0[Timer 0<br/>20 MHz]
        T1[Timer 1<br/>50 Hz / 14-bit]
        T2[Timer 2<br/>20 kHz / 10-bit]

        C0[Channel 0]
        C2[Channel 2]
        C3[Channel 3]
        C4[Channel 4]
        C5[Channel 5]

        T0 --> C0
        T1 --> C2
        T1 --> C3
        T2 --> C4
        T2 --> C5
    end

    C0 -->|Camera XCLK<br/>Phase 3| CAM[OV3660]
    C2 -->|GPIO44 PAN<br/>Phase 4| SRV1[Servo Pan]
    C3 -->|GPIO45 TILT<br/>Phase 4| SRV2[Servo Tilt]
    C4 -->|GPIO6 ENA<br/>Phase 5| MOT1[Motor Left]
    C5 -->|GPIO11 ENB<br/>Phase 5| MOT2[Motor Right]

    classDef timer fill:#cc4488,color:#fff
    classDef ch fill:#4488cc,color:#fff
    class T0,T1,T2 timer
    class C0,C2,C3,C4,C5 ch
```

---

## How to read these in Obsidian

- **Ctrl+P → Toggle Mermaid render**: switch raw ↔ rendered
- **Right-click diagram → Open in new tab**: fullscreen
- **Ctrl+, → Editor → Reading view**: always show rendered
- **Pan/zoom**: hold Ctrl + scroll wheel

For graphviz-style fancy layouts beyond Mermaid, install **Excalidraw** community plugin — best-in-class for whiteboard-style drawings.

---

## Diagram source references

| # | Diagram | Source spec |
|---|---------|-------------|
| 1 | System block | [[../PROJECT-SUMMARY#2-hardware-tổng-quan]] |
| 2 | Power tree | [[../hardware/power-tree-spec#1-kiến-trúc-tổng-thể]] |
| 3 | Task graph | [[../firmware/architecture#1-4-layer-stack]] |
| 4 | Behavior FSM | [[../firmware/architecture#5-top-level-behavior-fsm]] |
| 5 | Dock FSM | [[../hardware/dock-spec#4-state-machine-docking]] |
| 6 | Fall→SOS | [[../hardware/sensor-spec#4-fall-detection-algorithm]] + [[../hardware/sim800l-spec]] |
| 7 | MQTT topology | [[../deploy/home-assistant]] |
| 8 | Daily schedule | task_schedule defaults |
| 9 | LEDC allocation | [[../firmware/architecture#5-mapping-vào-kicad-schematic]] |
