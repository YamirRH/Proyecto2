import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import math

# --- FUNCIONES PSICROMÉTRICAS ---

def MPvs(T):
    if -100 <= T < 0:
        return [-5.6745359e3, 6.3925247, -9.677843e-3,
                6.2215701e-7, 2.0747825e-9, -9.484024e-13, 4.1635019]
    elif 0 <= T < 200:
        return [-5.8002206e3, 1.3914993, -4.8640239e-2,
                4.1764768e-5, -1.4452093e-8, 0.0, 6.5459673]
    else:
        return []

def Pvs(Tk, coef):
    if not coef: return np.nan
    return np.exp(coef[0]/Tk + coef[1] + coef[2]*Tk + coef[3]*Tk**2 +
                  coef[4]*Tk**3 + coef[5]*Tk**4 + coef[6]*np.log(Tk))

def Pv(HR,Pvs_val):
    return (HR/100)*Pvs_val

def W(P,Pv_val):
    # Evitar división por cero si Pv es igual a P (muy improbable en aire ambiente)
    if (P - Pv_val) == 0: return 0 
    return 0.621945*(Pv_val/(P-Pv_val))

def Ws(P,Pvs_val):
    return 0.621945*(Pvs_val/(P-Pvs_val))

def h(T,W_val):
    return 1.006*T+W_val*(2501+1.805*T)

def W_know_h(h_const, Tbs):
    return (h_const - 1.006 * Tbs) / (2501 + 1.805 * Tbs)

# --- CONFIGURACIÓN DE LA GRÁFICA ---

Z_graf = 2250 # msnm
P_graf = (101325.0 * math.pow((1 - (2.25577e-5) * Z_graf), 5.2259))

print(f"Presión para la gráfica: {P_graf:.2f} Pa")

Tbs_ejeX = np.linspace(-10, 55, 200) #rango 
Tk_ejeX = Tbs_ejeX + 273.15

fig, ax = plt.subplots(figsize=(14, 8))
ax.set_ylim(0, 0.035)
ax.set_xlim(-5, 55)

# --- DIBUJO DE CURVAS DE HUMEDAD RELATIVA ---
print("Dibujando curvas de Humedad Relativa...")
for hr in np.arange(10, 101, 10):
    vector_w_hr = []
    for k in Tbs_ejeX:
        coef_temp = MPvs(k)
        Tk_temp = k + 273.15
        pvs_temp = Pvs(Tk_temp, coef_temp)
        pv_temp = (hr / 100) * pvs_temp
        w_temp = W(P_graf, pv_temp)
        vector_w_hr.append(w_temp)
    
    if hr == 100:
        ax.plot(Tbs_ejeX, vector_w_hr, color='black', lw=2, label='Saturación (100% HR)')
    else:
        ax.plot(Tbs_ejeX, vector_w_hr, color='blue', linestyle='--', lw=0.7, alpha=0.6)
        
        # Etiquetas HR
        idx_label = np.searchsorted(vector_w_hr, 0.020) 
        if idx_label < len(Tbs_ejeX):
             ax.text(Tbs_ejeX[idx_label], vector_w_hr[idx_label], f'{hr}%', 
                     color='blue', fontsize=8, rotation=40, alpha=0.8)

# --- DIBUJO DE LÍNEAS DE BULBO HÚMEDO ---
print("Dibujando líneas de Bulbo Húmedo...")
for tbh_val in np.arange(-5, 40, 5):
    coef_sat = MPvs(tbh_val)
    pvs_sat = Pvs(tbh_val + 273.15, coef_sat)
    ws_sat = Ws(P_graf, pvs_sat)
    
    h_const = h(tbh_val, ws_sat)
    
    Tbs_line = Tbs_ejeX[Tbs_ejeX >= tbh_val]
    vector_w_tbh = W_know_h(h_const, Tbs_line)
            
    ax.plot(Tbs_line, vector_w_tbh, color='green', linestyle='-.', lw=0.6, alpha=0.5)

    if ws_sat > ax.get_ylim()[0] and ws_sat < ax.get_ylim()[1]:
        ax.text(tbh_val - 1.5, ws_sat + 0.0005, f'{tbh_val}°C', 
                color='green', ha='right', va='center', fontsize=8, weight='bold')

# --- PROCESAMIENTO DE DATOS ---

print("Leyendo archivo 'datalog2.txt' y procesando datos...")

try:
    df = pd.read_csv("datalog2.txt", sep=',', skipinitialspace=True)
    
    # Función auxiliar para calcular W dado un array de Tbs y HR
    def calcular_puntos(tbs_array, hr_array):
        w_res = []
        tbs_res = []
        for i in range(len(tbs_array)):
            tbs = tbs_array[i]
            hr = hr_array[i]
            
            # Validar que no sean NaNs
            if pd.isna(tbs) or pd.isna(hr):
                continue

            coef = MPvs(tbs)
            Tk = tbs + 273.15
            pvs_punto = Pvs(Tk, coef)
            pv_punto = Pv(hr, pvs_punto)
            w_punto = W(P_graf, pv_punto)
            
            tbs_res.append(tbs)
            w_res.append(w_punto)
        return tbs_res, w_res

    # 1. Datos DHT11 (Tbs2_C, phi2_%)
    if 'Tbs2_C' in df.columns and 'phi2_%' in df.columns:
        tbs_dht, w_dht = calcular_puntos(df['Tbs2_C'].values, df['phi2_%'].values)
        
        ax.scatter(tbs_dht, w_dht, 
                   color='magenta', s=20, edgecolor='black', linewidth=0.5, alpha=0.8, zorder=10, 
                   label='DHT11 (T2, H2)')
    else:
        print("Advertencia: No se encontraron columnas de DHT11 (Tbs2_C, phi2_%)")

    # 2. Datos Termistores (Tbs_C, phi_%)
    if 'Tbs_C' in df.columns and 'phi_%' in df.columns:
        tbs_term, w_term = calcular_puntos(df['Tbs_C'].values, df['phi_%'].values)
        
        ax.scatter(tbs_term, w_term, 
                   color='red', s=20, marker='s', edgecolor='black', linewidth=0.5, alpha=0.8, zorder=10, 
                   label='Termistores (T1, H1)')
    else:
        print("Advertencia: No se encontraron columnas de Termistores (Tbs_C, phi_%)")

except FileNotFoundError:
    print("ERROR: No se encontró el archivo 'datalog2.txt'.")
except Exception as e:
    print(f"ERROR al procesar datos: {e}")

# --- AJUSTES FINALES DEL GRÁFICO ---
ax.set_xlabel("Temperatura de Bulbo Seco (°C)", fontsize=12)
ax.set_ylabel("Relación de Humedad (kg agua / kg aire seco)", fontsize=12)
ax.set_title(f"Carta Psicrométrica Comparativa (Z={Z_graf}m)", fontsize=14, weight='bold')
ax.grid(True, linestyle=':', alpha=0.6)
ax.legend(loc='upper left', frameon=True, shadow=True)

plt.tight_layout()
print("Mostrando gráfica...")
plt.show()