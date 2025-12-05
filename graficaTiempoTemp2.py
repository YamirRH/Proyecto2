import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

# --- 1. Cargar los Datos ---
nombre_archivo = 'datalog2.txt'

print(f"Leyendo datos de {nombre_archivo}...")

try:
    # Usamos skipinitialspace=True para manejar el formato "dato, dato" correctamente
    df = pd.read_csv(nombre_archivo, sep=',', skipinitialspace=True)
    
    # Convertir la columna Timestamp a objetos de fecha real para que la gráfica entienda el tiempo
    df['Timestamp'] = pd.to_datetime(df['Timestamp'])
    
    # --- 2. Configurar la Gráfica ---
    fig, ax = plt.subplots(figsize=(12, 6))
    
    # Graficar Tbs (Bulbo Seco - Sensor 1)
    ax.plot(df['Timestamp'], df['Tbs_C'], 
            color='red', linewidth=1, alpha=0.8, label='Tbs (Bulbo Seco - Termistor)')
    
    # Graficar Tbs2 (Bulbo Seco - Dht11)
    ax.plot(df['Timestamp'], df['Tbs2_C'], 
            color='green', linewidth=1.7, linestyle='--', alpha=0.7, label='Tbs2 (Bulbo Seco - DHT11)')
            
    # Graficar Tbh (Bulbo Húmedo - Sensor 2)
    ax.plot(df['Timestamp'], df['Tbh_C'], 
            color='yellow', linewidth=1, alpha=0.8, label='Tbh (Bulbo Húmedo) - Termistor')
    
    # Graficar TbsProt (Bulbo Seco Protegido - Sensor 3)
    ax.plot(df['Timestamp'], df['TbsProtect_C'], 
            color='black', linewidth=1, alpha=0.8, label='Tbs Protegido (Bulbo Seco) - Termistor')
    
    # Graficar TbsSol (Bulbo Seco al Sol - Sensor 4)
    ax.plot(df['Timestamp'], df['TbsUnprotect_C'], 
            color='orange', linewidth=1, alpha=0.8, label='Tbs Sol (Bulbo Seco) - Termistor')
    
    # --- 3. Formato y Estilo ---
    ax.set_title("Comparación de Temperaturas en el Tiempo", fontsize=14, weight='bold')
    ax.set_xlabel("Tiempo", fontsize=12)
    ax.set_ylabel("Temperatura (°C)", fontsize=12)
    
    # Formato del eje X (Fechas)
    # Muestra día y hora (ej: 18/11 12:00)
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%d/%m %H:%M'))
    fig.autofmt_xdate() # Rota las fechas para que no se encimen
    
    ax.grid(True, linestyle=':', alpha=0.6)
    ax.legend(loc='lower right', shadow=True)
    
    plt.tight_layout()
    
    print("Mostrando gráfica...")
    plt.show()

except FileNotFoundError:
    print(f"ERROR: No se encontró el archivo '{nombre_archivo}'. Asegúrate de haberlo generado primero.")
except Exception as e:
    print(f"ERROR al procesar los datos: {e}")