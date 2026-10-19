<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="es_ES">
<context>
    <name>BackgroundImage</name>
    <message>
        <location filename="../src/comp/BackgroundImage.cpp" line="56"/>
        <source>Could not read an image from %1</source>
        <translation>No se pudo leer una imagen desde %1</translation>
    </message>
</context>
<context>
    <name>OpenCvVideoDecoder</name>
    <message>
        <location filename="../src/comp/OpenCvVideoDecoder.cpp" line="14"/>
        <source>Could not open a video from %1</source>
        <translation>No se pudo abrir un vídeo desde %1</translation>
    </message>
    <message>
        <location filename="../src/comp/OpenCvVideoDecoder.cpp" line="27"/>
        <source>Could not read a frame from %1</source>
        <translation>No se pudo leer un fotograma desde %1</translation>
    </message>
</context>
<context>
    <name>PkexecLoopbackAdmin</name>
    <message>
        <location filename="../src/sink/PkexecLoopbackAdmin.cpp" line="54"/>
        <source>The v4l2loopback kernel module is not installed on this system. Install it with: %1</source>
        <translation>El módulo del núcleo v4l2loopback no está instalado en este sistema. Instálelo con: %1</translation>
    </message>
    <message>
        <location filename="../src/sink/PkexecLoopbackAdmin.cpp" line="164"/>
        <source>v4l2loopback is already loaded by another program, without this program&apos;s virtual camera. It was left as it is; the virtual camera will be there after the next reboot.</source>
        <translation>v4l2loopback ya está cargado por otro programa, sin la cámara virtual de este programa. Se ha dejado como está; la cámara virtual estará disponible tras el próximo reinicio.</translation>
    </message>
    <message>
        <location filename="../src/sink/PkexecLoopbackAdmin.cpp" line="174"/>
        <source>Could not get permission to set up the virtual camera. Run this in a terminal instead:
%1</source>
        <translation>No se pudo obtener permiso para configurar la cámara virtual. Ejecute esto en un terminal:
%1</translation>
    </message>
    <message>
        <location filename="../src/sink/PkexecLoopbackAdmin.cpp" line="183"/>
        <location filename="../src/sink/PkexecLoopbackAdmin.cpp" line="191"/>
        <source>Loading the virtual camera module was cancelled or failed</source>
        <translation>La carga del módulo de la cámara virtual se canceló o falló</translation>
    </message>
    <message>
        <location filename="../src/sink/PkexecLoopbackAdmin.cpp" line="202"/>
        <source>The module was loaded but no device labelled %1 appeared</source>
        <translation>El módulo se cargó pero no apareció ningún dispositivo con la etiqueta %1</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="../src/main.cpp" line="69"/>
        <location filename="../src/main.cpp" line="82"/>
        <source>Webcam Background Changer</source>
        <translation>Webcam Background Changer</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="70"/>
        <source>The segmentation model was not found. It is looked for in:
%1</source>
        <translation>No se encontró el modelo de segmentación. Se busca en:
%1</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="83"/>
        <source>The segmentation model could not be loaded:
%1</source>
        <translation>No se pudo cargar el modelo de segmentación:
%1</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="108"/>
        <source>Choose a background video</source>
        <translation>Elegir un vídeo de fondo</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="109"/>
        <source>Videos (*.mp4 *.mkv *.webm *.mov *.avi)</source>
        <translation>Vídeos (*.mp4 *.mkv *.webm *.mov *.avi)</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="111"/>
        <source>Choose a background picture</source>
        <translation>Elegir una imagen de fondo</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="113"/>
        <source>Images (*.png *.jpg *.jpeg *.bmp *.webp)</source>
        <translation>Imágenes (*.png *.jpg *.jpeg *.bmp *.webp)</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="116"/>
        <source>Choose a background colour</source>
        <translation>Elegir un color de fondo</translation>
    </message>
</context>
<context>
    <name>V4l2CameraSource</name>
    <message>
        <location filename="../src/camera/V4l2CameraSource.cpp" line="89"/>
        <source>The device offered no usable capture format: %1</source>
        <translation>El dispositivo no ofreció ningún formato de captura utilizable: %1</translation>
    </message>
    <message>
        <location filename="../src/camera/V4l2CameraSource.cpp" line="98"/>
        <source>Could not open %1; it may be in use by another application</source>
        <translation>No se pudo abrir %1; puede que otra aplicación lo esté usando</translation>
    </message>
    <message>
        <location filename="../src/camera/V4l2CameraSource.cpp" line="130"/>
        <source>The camera is not open</source>
        <translation>La cámara no está abierta</translation>
    </message>
    <message>
        <location filename="../src/camera/V4l2CameraSource.cpp" line="134"/>
        <source>The camera stopped delivering frames; it may have been disconnected</source>
        <translation>La cámara dejó de entregar fotogramas; puede que se haya desconectado</translation>
    </message>
</context>
<context>
    <name>V4l2LoopbackSink</name>
    <message>
        <location filename="../src/sink/V4l2LoopbackSink.cpp" line="30"/>
        <source>Could not open the virtual camera %1: %2</source>
        <translation>No se pudo abrir la cámara virtual %1: %2</translation>
    </message>
    <message>
        <location filename="../src/sink/V4l2LoopbackSink.cpp" line="48"/>
        <source>The virtual camera refused the format: %1</source>
        <translation>La cámara virtual rechazó el formato: %1</translation>
    </message>
    <message>
        <location filename="../src/sink/V4l2LoopbackSink.cpp" line="77"/>
        <source>The virtual camera is not open</source>
        <translation>La cámara virtual no está abierta</translation>
    </message>
    <message>
        <location filename="../src/sink/V4l2LoopbackSink.cpp" line="82"/>
        <source>The frame does not match the geometry the virtual camera was opened with</source>
        <translation>El fotograma no coincide con la geometría con la que se abrió la cámara virtual</translation>
    </message>
    <message>
        <location filename="../src/sink/V4l2LoopbackSink.cpp" line="93"/>
        <source>Could not convert the frame: %1</source>
        <translation>No se pudo convertir el fotograma: %1</translation>
    </message>
    <message>
        <location filename="../src/sink/V4l2LoopbackSink.cpp" line="120"/>
        <source>short write</source>
        <translation>escritura incompleta</translation>
    </message>
    <message>
        <location filename="../src/sink/V4l2LoopbackSink.cpp" line="121"/>
        <source>Writing to the virtual camera failed: %1</source>
        <translation>Falló la escritura en la cámara virtual: %1</translation>
    </message>
</context>
<context>
    <name>wbc::MainWindow</name>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="687"/>
        <source>Start</source>
        <translation>Iniciar</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="441"/>
        <source>%1 fps · %2 ms · %3 dropped</source>
        <translation>%1 fps · %2 ms · %3 descartados</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="451"/>
        <source>Virtual camera: %1 · capturing %2x%3 at %4 fps</source>
        <translation>Cámara virtual: %1 · capturando %2x%3 a %4 fps</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="519"/>
        <source>The camera you had selected is no longer present. %1 is selected instead.</source>
        <translation>La cámara que había seleccionado ya no está presente. En su lugar se ha seleccionado %1.</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="526"/>
        <source>Virtual camera: %1</source>
        <translation>Cámara virtual: %1</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="537"/>
        <source>Virtual camera: not set up yet</source>
        <translation>Cámara virtual: aún no está configurada</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="568"/>
        <source>Run this to create it:
%1</source>
        <translation>Ejecute esto para crearla:
%1</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="538"/>
        <source>Virtual camera: the kernel module is not installed</source>
        <translation>Cámara virtual: el módulo del núcleo no está instalado</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="547"/>
        <source>This copy of the program runs in a Flatpak sandbox, from which it cannot set up the virtual camera. Run these commands on your system, outside the sandbox:</source>
        <translation>Esta copia del programa se ejecuta en un entorno aislado de Flatpak, desde el que no puede configurar la cámara virtual. Ejecute estos comandos en su sistema, fuera del entorno aislado:</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="554"/>
        <source>Install it first:
%1</source>
        <translation>Instálelo primero:
%1</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="564"/>
        <source>Run this in a terminal to set it up:</source>
        <translation>Ejecute esto en un terminal para configurarla:</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="623"/>
        <source>Blur strength: %1 px</source>
        <translation>Intensidad del desenfoque: %1 px</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="624"/>
        <source>Cut off background below: %1%</source>
        <translation>Recortar el fondo por debajo de: %1%</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="625"/>
        <source>Steady the mask over time: %1%</source>
        <translation>Estabilizar la máscara en el tiempo: %1%</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="626"/>
        <source>Follow the edges in the frame: %1 px</source>
        <translation>Seguir los bordes del fotograma: %1 px</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="630"/>
        <source>Webcam Background Changer</source>
        <translation>Webcam Background Changer</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="631"/>
        <source>Camera:</source>
        <translation>Cámara:</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="632"/>
        <source>Language:</source>
        <translation>Idioma:</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="633"/>
        <source>Set up the virtual camera</source>
        <translation>Configurar la cámara virtual</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="634"/>
        <source>Copy</source>
        <translation>Copiar</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="635"/>
        <source>Background</source>
        <translation>Fondo</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="636"/>
        <source>Blur</source>
        <translation>Desenfoque</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="637"/>
        <source>Picture</source>
        <translation>Imagen</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="638"/>
        <source>Solid colour</source>
        <translation>Color sólido</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="639"/>
        <source>Video</source>
        <translation>Vídeo</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="640"/>
        <source>Choose a picture...</source>
        <translation>Elegir una imagen...</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="641"/>
        <source>Choose a colour...</source>
        <translation>Elegir un color...</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="642"/>
        <source>Choose a video...</source>
        <translation>Elegir un vídeo...</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="643"/>
        <source>Mask</source>
        <translation>Máscara</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="645"/>
        <source>No preview yet</source>
        <translation>Todavía no hay vista previa</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="687"/>
        <source>Stop</source>
        <translation>Detener</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="706"/>
        <source>Now choose %1 in the other application. If it was already open before you pressed Start, restart it first.</source>
        <translation>Ahora elija %1 en la otra aplicación. Si ya estaba abierta antes de pulsar Iniciar, reiníciela primero.</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="710"/>
        <source>Other applications list %1 only while this window is streaming. Press Start here first, then choose it there.</source>
        <translation>Otras aplicaciones solo ven %1 mientras esta ventana está transmitiendo. Pulse Iniciar aquí primero y luego elíjala allí.</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="724"/>
        <source>No camera was found. Connect one and press the button again.</source>
        <translation>No se encontró ninguna cámara. Conecte una y pulse el botón de nuevo.</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="728"/>
        <source>The virtual camera is not set up yet; see the note above.</source>
        <translation>La cámara virtual aún no está configurada; consulte la nota de arriba.</translation>
    </message>
</context>
</TS>
