<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ru_RU">
<context>
    <name>BackgroundImage</name>
    <message>
        <location filename="../src/comp/BackgroundImage.cpp" line="56"/>
        <source>Could not read an image from %1</source>
        <translation>Не удалось прочитать изображение из %1</translation>
    </message>
</context>
<context>
    <name>OpenCvVideoDecoder</name>
    <message>
        <location filename="../src/comp/OpenCvVideoDecoder.cpp" line="14"/>
        <source>Could not open a video from %1</source>
        <translation>Не удалось открыть видео из %1</translation>
    </message>
    <message>
        <location filename="../src/comp/OpenCvVideoDecoder.cpp" line="27"/>
        <source>Could not read a frame from %1</source>
        <translation>Не удалось прочитать кадр из %1</translation>
    </message>
</context>
<context>
    <name>PkexecLoopbackAdmin</name>
    <message>
        <location filename="../src/sink/PkexecLoopbackAdmin.cpp" line="54"/>
        <source>The v4l2loopback kernel module is not installed on this system. Install it with: %1</source>
        <translation>Модуль ядра v4l2loopback не установлен в системе. Установите его командой: %1</translation>
    </message>
    <message>
        <location filename="../src/sink/PkexecLoopbackAdmin.cpp" line="164"/>
        <source>v4l2loopback is already loaded by another program, without this program&apos;s virtual camera. It was left as it is; the virtual camera will be there after the next reboot.</source>
        <translation>Модуль v4l2loopback уже загружен другой программой, без виртуальной камеры этой программы. Он оставлен как есть; виртуальная камера появится после следующей перезагрузки.</translation>
    </message>
    <message>
        <location filename="../src/sink/PkexecLoopbackAdmin.cpp" line="174"/>
        <source>Could not get permission to set up the virtual camera. Run this in a terminal instead:
%1</source>
        <translation>Не удалось получить разрешение на настройку виртуальной камеры. Выполните в терминале:
%1</translation>
    </message>
    <message>
        <location filename="../src/sink/PkexecLoopbackAdmin.cpp" line="183"/>
        <location filename="../src/sink/PkexecLoopbackAdmin.cpp" line="191"/>
        <source>Loading the virtual camera module was cancelled or failed</source>
        <translation>Загрузка модуля виртуальной камеры отменена или не удалась</translation>
    </message>
    <message>
        <location filename="../src/sink/PkexecLoopbackAdmin.cpp" line="202"/>
        <source>The module was loaded but no device labelled %1 appeared</source>
        <translation>Модуль загружен, но устройство с именем %1 не появилось</translation>
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
        <translation>Модель сегментации не найдена. Места, где она ищется:
%1</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="83"/>
        <source>The segmentation model could not be loaded:
%1</source>
        <translation>Не удалось загрузить модель сегментации:
%1</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="108"/>
        <source>Choose a background video</source>
        <translation>Выберите видео для фона</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="109"/>
        <source>Videos (*.mp4 *.mkv *.webm *.mov *.avi)</source>
        <translation>Видео (*.mp4 *.mkv *.webm *.mov *.avi)</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="111"/>
        <source>Choose a background picture</source>
        <translation>Выберите картинку для фона</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="113"/>
        <source>Images (*.png *.jpg *.jpeg *.bmp *.webp)</source>
        <translation>Изображения (*.png *.jpg *.jpeg *.bmp *.webp)</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="116"/>
        <source>Choose a background colour</source>
        <translation>Выберите цвет фона</translation>
    </message>
</context>
<context>
    <name>V4l2CameraSource</name>
    <message>
        <location filename="../src/camera/V4l2CameraSource.cpp" line="89"/>
        <source>The device offered no usable capture format: %1</source>
        <translation>Устройство не предложило ни одного пригодного формата захвата: %1</translation>
    </message>
    <message>
        <location filename="../src/camera/V4l2CameraSource.cpp" line="98"/>
        <source>Could not open %1; it may be in use by another application</source>
        <translation>Не удалось открыть %1; возможно, устройство занято другим приложением</translation>
    </message>
    <message>
        <location filename="../src/camera/V4l2CameraSource.cpp" line="130"/>
        <source>The camera is not open</source>
        <translation>Камера не открыта</translation>
    </message>
    <message>
        <location filename="../src/camera/V4l2CameraSource.cpp" line="134"/>
        <source>The camera stopped delivering frames; it may have been disconnected</source>
        <translation>Камера перестала выдавать кадры; возможно, её отключили</translation>
    </message>
</context>
<context>
    <name>V4l2LoopbackSink</name>
    <message>
        <location filename="../src/sink/V4l2LoopbackSink.cpp" line="30"/>
        <source>Could not open the virtual camera %1: %2</source>
        <translation>Не удалось открыть виртуальную камеру %1: %2</translation>
    </message>
    <message>
        <location filename="../src/sink/V4l2LoopbackSink.cpp" line="48"/>
        <source>The virtual camera refused the format: %1</source>
        <translation>Виртуальная камера отвергла формат: %1</translation>
    </message>
    <message>
        <location filename="../src/sink/V4l2LoopbackSink.cpp" line="77"/>
        <source>The virtual camera is not open</source>
        <translation>Виртуальная камера не открыта</translation>
    </message>
    <message>
        <location filename="../src/sink/V4l2LoopbackSink.cpp" line="82"/>
        <source>The frame does not match the geometry the virtual camera was opened with</source>
        <translation>Кадр не совпадает с геометрией, с которой открыта виртуальная камера</translation>
    </message>
    <message>
        <location filename="../src/sink/V4l2LoopbackSink.cpp" line="93"/>
        <source>Could not convert the frame: %1</source>
        <translation>Не удалось преобразовать кадр: %1</translation>
    </message>
    <message>
        <location filename="../src/sink/V4l2LoopbackSink.cpp" line="120"/>
        <source>short write</source>
        <translation>неполная запись</translation>
    </message>
    <message>
        <location filename="../src/sink/V4l2LoopbackSink.cpp" line="121"/>
        <source>Writing to the virtual camera failed: %1</source>
        <translation>Запись в виртуальную камеру не удалась: %1</translation>
    </message>
</context>
<context>
    <name>wbc::MainWindow</name>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="687"/>
        <source>Start</source>
        <translation>Пуск</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="441"/>
        <source>%1 fps · %2 ms · %3 dropped</source>
        <translation>%1 fps · %2 мс · %3 пропущено</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="451"/>
        <source>Virtual camera: %1 · capturing %2x%3 at %4 fps</source>
        <translation>Виртуальная камера: %1 · захват %2x%3 при %4 fps</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="519"/>
        <source>The camera you had selected is no longer present. %1 is selected instead.</source>
        <translation>Выбранной камеры больше нет. Вместо неё выбрана %1.</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="526"/>
        <source>Virtual camera: %1</source>
        <translation>Виртуальная камера: %1</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="537"/>
        <source>Virtual camera: not set up yet</source>
        <translation>Виртуальная камера: ещё не создана</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="568"/>
        <source>Run this to create it:
%1</source>
        <translation>Выполните, чтобы создать её:
%1</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="538"/>
        <source>Virtual camera: the kernel module is not installed</source>
        <translation>Виртуальная камера: модуль ядра не установлен</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="547"/>
        <source>This copy of the program runs in a Flatpak sandbox, from which it cannot set up the virtual camera. Run these commands on your system, outside the sandbox:</source>
        <translation>Эта копия программы работает в песочнице Flatpak и не может сама настроить виртуальную камеру. Выполните эти команды в системе, вне песочницы:</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="554"/>
        <source>Install it first:
%1</source>
        <translation>Сначала установите его:
%1</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="564"/>
        <source>Run this in a terminal to set it up:</source>
        <translation>Выполните это в терминале, чтобы настроить её:</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="623"/>
        <source>Blur strength: %1 px</source>
        <translation>Сила размытия: %1 px</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="624"/>
        <source>Cut off background below: %1%</source>
        <translation>Отсекать фон ниже: %1%</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="625"/>
        <source>Steady the mask over time: %1%</source>
        <translation>Стабилизировать маску во времени: %1%</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="626"/>
        <source>Follow the edges in the frame: %1 px</source>
        <translation>Следовать краям в кадре: %1 px</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="630"/>
        <source>Webcam Background Changer</source>
        <translation>Webcam Background Changer</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="631"/>
        <source>Camera:</source>
        <translation>Камера:</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="632"/>
        <source>Language:</source>
        <translation>Язык:</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="633"/>
        <source>Set up the virtual camera</source>
        <translation>Создать виртуальную камеру</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="634"/>
        <source>Copy</source>
        <translation>Копировать</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="635"/>
        <source>Background</source>
        <translation>Фон</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="636"/>
        <source>Blur</source>
        <translation>Размытие</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="637"/>
        <source>Picture</source>
        <translation>Картинка</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="638"/>
        <source>Solid colour</source>
        <translation>Сплошной цвет</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="639"/>
        <source>Video</source>
        <translation>Видео</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="640"/>
        <source>Choose a picture...</source>
        <translation>Выбрать картинку...</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="641"/>
        <source>Choose a colour...</source>
        <translation>Выбрать цвет...</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="642"/>
        <source>Choose a video...</source>
        <translation>Выбрать видео...</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="643"/>
        <source>Mask</source>
        <translation>Маска</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="645"/>
        <source>No preview yet</source>
        <translation>Превью пока нет</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="687"/>
        <source>Stop</source>
        <translation>Стоп</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="706"/>
        <source>Now choose %1 in the other application. If it was already open before you pressed Start, restart it first.</source>
        <translation>Теперь выберите %1 в другом приложении. Если оно было открыто до нажатия «Пуск», сначала перезапустите его.</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="710"/>
        <source>Other applications list %1 only while this window is streaming. Press Start here first, then choose it there.</source>
        <translation>Другие приложения видят %1 только пока это окно передаёт видео. Сначала нажмите «Пуск» здесь, затем выберите её там.</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="724"/>
        <source>No camera was found. Connect one and press the button again.</source>
        <translation>Камера не найдена. Подключите её и нажмите кнопку снова.</translation>
    </message>
    <message>
        <location filename="../src/ui/MainWindow.cpp" line="728"/>
        <source>The virtual camera is not set up yet; see the note above.</source>
        <translation>Виртуальная камера ещё не создана; см. примечание выше.</translation>
    </message>
</context>
</TS>
