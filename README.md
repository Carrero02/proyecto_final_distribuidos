# Estructura del proyecto

## Archivos

### Servidor
- `server.c`: Código fuente del servidor.
- `./server_functions`: Directorio que contiene los archivos de las funciones del servidor:
  - `server_functions.c`: Código fuente de las funciones del servidor.
  - `server_functions.h`: Cabecera de las funciones del servidor.
- `./rpc_server`: Directorio que contiene los archivos del servidor RPC:
  - `rpc_server.c`: Código fuente del servidor RPC.
  - `rpc_server.x`: Archivo de definición de las funciones RPC.
  - `./rpc_server_functions`: Directorio que contiene los archivos de las funciones RPC:
    - `rpc_server_functions.c`: Código fuente de las funciones del servidor RPC.
    - `rpc_server_functions.h`: Cabecera de las funciones del servidor RPC.

### Cliente
- `client.py`: Código fuente del cliente.
- `requirements.txt`: Archivo que contiene las dependencias necesarias para el cliente.
- `./web_service`: Directorio que contiene los archivos del servicio web:
  - `time_ws.py`: Código fuente del servicio web.

## Procesos
- **Cliente**: Realiza las peticiones al servidor, pasándole también la fecha y hora actual, obtenidas del servicio web. También se comunica con otros clientes peer-to-peer en la transferencia de archivos.
- **Servicio web**: Proporciona la fecha y hora del momento en el que recibe una petición de un cliente.
- **Servidor**: Recibe las peticiones de los clientes y las procesa.
- **Servidor RPC**: Recibe desde el servidor las operaciones que realizan los clientes, así como el usuario que las realiza y la fecha y hora en la que se realizan. Se encarga de imprimir en pantalla el registro de las operaciones realizadas por los usuarios.


# ¿Cómo compilar el proyecto?
Desde la raíz del proyecto, ejecutar el siguiente comando:
```
make
```

Si se quiere limpiar los archivos generados por la compilación, así como los archivos de usuarios registrados y publicaciones, ejecutar el siguiente comando:
```
make clean
```
# ¿Cómo ejecutar el proyecto?
## Servidor
### Servidor - Terminal 1
Desde la raíz del proyecto, ejecutar los siguientes comandos:
```
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.
./server -p <puerto del servidor>
```
### Servidor - Terminal 2
Desde la raíz del proyecto, ejecutar el siguiente comando:
```
./rpc_server_exe
```
## Cliente
Se recomienda crear y activar un entorno virtual para el cliente:
```
python3 -m venv venv
source venv/bin/activate
```

Antes que nada, se han de instalar las dependencias necesarias para el cliente. Para ello, ejecutar el siguiente comando:
```
pip3 install -r requirements.txt
```
### Cliente - Terminal 1
Desde la raíz del proyecto, ejecutar el siguiente comando:
```
python3 ./web_service/time_ws.py
```
### Cliente - Terminal 2
Desde la raíz del proyecto, ejecutar el siguiente comando:
```
python3 client.py -s <ip del servidor> -p <puerto del servidor>
```

### Cliente - Terminal 3, 4, ...
Para ejecutar más clientes, repetir el proceso de la Terminal 2 del cliente:
```
python3 client.py -s <ip del servidor> -p <puerto del servidor>
```

### Nota
La IP y el puerto del servicio web están definidos en el archivo `client.py` en el atributo `client._ws_url` con la siguiente URL: `"http://localhost:5000/get_time"`. Por lo tanto, el puerto 5000 de la máquina donde se ejecute el cliente debe estar libre. Además, el servicio web se debe ejecutar en la misma máquina donde se ejecute el cliente. Se pueden ejecutar varios clientes en la misma máquina, la única condición es que el servicio web también se esté´ejecutando se ejecute en esa máquina.

Si esto se desea cambiar, para poder correr el servicio web en una máquina dedicada, se debe modificar el archivo `client.py` en el atributo `client._ws_url` con la URL correspondiente, así como el archivo `time_ws.py` en los atributos del método `app.run()` con la IP y puerto correspondientes.
