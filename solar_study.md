# Solar Feasibility Study: TempSensor Power System

This study evaluates the technical feasibility of powering the continuously running **TempSensor** module using your **Powertech ZM9093 10W Solar Panel** and the **LOLIN D1 mini Pro** onboard charger.

---

## 1. Component Specifications

### Solar Panel: Powertech ZM9093 (10W)
*   **Max Rated Power ($P_{max}$):** 10W
*   **Optimal Operating Voltage ($V_{mp}$):** 17.2V
*   **Optimal Operating Current ($I_{mp}$):** 0.58A
*   **Open Circuit Voltage ($V_{oc}$):** 21.6V
*   **Short Circuit Current ($I_{sc}$):** 0.64A

### Charging Circuit: LOLIN D1 mini Pro Onboard Charger
*   **Charger IC:** TP4054 (Constant-Current/Constant-Voltage Linear LiPo Charger)
*   **Max Charging Current:** 500mA (0.5A)
*   **Battery Chemistry:** Single-Cell 3.7V Lithium-Polymer (LiPo) or Lithium-Ion (4.2V charge termination)

---

## 2. Power Input Constraints & Risks

> [!CAUTION]
> **DO NOT connect the Solar Panel directly to the D1 mini Pro (USB or 5V pin).**
> Doing so will instantly destroy the board. Here is why:
>
> 1.  **Overvoltage Limit:** The TP4054 charger IC has an **Absolute Maximum VCC rating of 10V**. The solar panel operates at **17.2V** ($V_{mp}$) and can rise up to **21.6V** ($V_{oc}$) when the battery is full.
> 2.  **LDO Regulator Limit:** The board's onboard 3.3V LDO regulator (ME6211) has an absolute max input of **6.0V–6.5V**.
> 3.  **Linear Charger Heat Dissipation:** The TP4054 is a linear regulator. It burns off excess input voltage as heat. Even if it survived 10V, dropping 10V to a 3.7V battery at 500mA would generate $3.15\text{W}$ of heat, triggering immediate thermal shutdown or chip failure.

### Maximum Power Input Conditions for the D1 mini Pro Charger
To charge safely and efficiently through the D1 mini Pro board, the input supply to the USB/5V pin must satisfy:
*   **Recommended Voltage:** $4.5\text{V} \le V_{in} \le 5.5\text{V}$ (standard 5.0V USB level)
*   **Absolute Maximum Voltage:** 6.0V (to prevent ME6211 regulator failure)
*   **Optimal Input Current:** $\ge 650\text{mA}$ (to support $500\text{mA}$ charge current + $120\text{mA}$ active board operation simultaneously)

---

## 3. Voltage Regulation Recommendation

To bridge the **17.2V Solar Panel** to the **5V D1 mini Pro Input**, you **MUST** place a regulator between them. 

### Recommendation: High-Efficiency DC-to-DC Buck Converter
Use a step-down switching regulator (e.g., LM2596, MP1584, or a standard 12V/24V USB car charger adapter) to step down the solar panel's 17.2V output to 5V.

*   **Why a Buck Converter?** A linear regulator (like an LM7805) would discard the excess 12.2V as heat, yielding only **29% efficiency**. A buck converter operates at **90% efficiency**, translating the high voltage into higher usable output current.
*   **Buck Output Calculation:**
    $$\text{Panel Power Output} = 17.2\text{V} \times 0.58\text{A} = 10\text{W}$$
    $$\text{Buck Output Power (90% Eff.)} = 10\text{W} \times 0.90 = 9\text{W}$$
    $$\text{Available Charging Current at 5V} = \frac{9\text{W}}{5\text{V}} = 1.8\text{A}$$
    *This provides a robust $1.8\text{A}$ at 5.0V, which easily satisfies the $650\text{mA}$ needed to run the server and charge the battery at the maximum 500mA limit.*

---

## 4. Daily Power Balance Calculations

Here we calculate if a **4–6 hour daily sunlight window** (average 5 hours) can sustain continuous active operation (no deep sleep, local web server active).

### Daily Power Consumption
*   **Average Continuous Current Draw:** $\sim 100\text{mA}$ (ESP8266 Wi-Fi active + OLED + BME280 + SD card idle)
*   **Voltage:** 3.7V nominal LiPo
*   **Daily Energy Consumed:**
    $$E_{cons} = 100\text{mA} \times 24\text{h} \times 3.7\text{V} = 8.88\text{Wh (or 2400mAh)}$$

### Daily Solar Power Generation
Assuming an average of **5 hours of full equivalent sun** per day, and accounting for real-world losses (panel angle, dust, temperature coefficient, and charger conversion losses) with a conservative **50% total system efficiency**:
*   **Daily Energy Generated:**
    $$E_{gen} = 10\text{W (Panel)} \times 5\text{h} \times 0.5\text{ efficiency} = 25.0\text{Wh}$$
*   **Conversion to LiPo Capacity (3.7V):**
    $$Q_{gen} = \frac{25.0\text{Wh}}{3.7\text{V}} \approx 6750\text{mAh}$$

### Power Balance Summary

| Metric | Value | Status |
| :--- | :--- | :--- |
| **Daily Consumption** | 8.88 Wh (2400 mAh) | |
| **Daily Generation (10W Panel)** | 25.00 Wh (6750 mAh) | |
| **Net Daily Balance** | **+16.12 Wh (+4350 mAh)** | **Net Positive Surplus (2.8x safety margin)** |

---

## 5. Conclusion & Solar Study Recommendations

1.  **Highly Feasible:** Your 10W panel is more than capable of keeping the TempSensor running continuously. It provides a **$2.8\times$ power surplus**, meaning it will keep the battery fully charged even on heavily overcast or rainy days.
2.  **Regulator Required:** You must acquire a **5V step-down DC-to-DC buck regulator** (or a 12V/24V USB car outlet module). Connect the Solar Panel (+) and (-) terminals to the regulator input, and feed the regulator's 5V USB output to the D1 mini Pro's USB port (or 5V/GND pins).
3.  **Battery Size:** Since the device consumes $2400\text{mAh}$ per day, you should use a **LiPo battery of at least 3000mAh–4000mAh**. This provides more than 24 hours of autonomous reserve capacity to carry the system through consecutive rainy days with zero sunlight.
