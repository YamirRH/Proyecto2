#grafica comparacion HumedRelt
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
    
    # Graficar Tbs (HR DHT11 - Sensor 1)
    ax.plot(df['Timestamp'], df['phi2_%'], 
            color='red', linewidth=1, alpha=0.8, label='HR (DHT11)')
    # Graficar Tbs (HR  - calculado)
    ax.plot(df['Timestamp'], df['phi_%'], 
            color='blue', linewidth=1, alpha=0.8, label='HR (Calculada)')

            

    # --- 3. Formato y Estilo ---
    ax.set_title("Comparación de HR en el Tiempo", fontsize=14, weight='bold')
    ax.set_xlabel("Tiempo", fontsize=12)
    ax.set_ylabel("Humedad Relativa (%)", fontsize=12)
    
    # Formato del eje X (Fechas)
    # Muestra día y hora (ej: 18/11 12:00)
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%d/%m %H:%M'))
    fig.autofmt_xdate() # Rota las fechas para que no se encimen
    
    ax.grid(True, linestyle=':', alpha=0.6)
    ax.legend(loc='upper right', shadow=True)
    
    plt.tight_layout()
    
    print("Mostrando gráfica...")
    plt.show()

except FileNotFoundError:
    print(f"ERROR: No se encontró el archivo '{nombre_archivo}'. Asegúrate de haberlo generado primero.")
except Exception as e:
    print(f"ERROR al procesar los datos: {e}")