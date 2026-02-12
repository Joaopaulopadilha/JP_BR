// camera.hpp
// Módulo de captura de câmera para biblioteca CVN (Portável Windows/Linux)

#ifndef CVN_CAMERA_HPP
#define CVN_CAMERA_HPP

#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>

// === PLATAFORMA WINDOWS (DirectShow) ===
#ifdef _WIN32

#include <windows.h>
#include <dshow.h>

#pragma comment(lib, "strmiids.lib")

static const GUID CLSID_SampleGrabber = {0xC1F400A0, 0x3F08, 0x11d3, {0x9F, 0x0B, 0x00, 0x60, 0x08, 0x03, 0x9E, 0x37}};
static const GUID CLSID_NullRenderer = {0xC1F400A4, 0x3F08, 0x11d3, {0x9F, 0x0B, 0x00, 0x60, 0x08, 0x03, 0x9E, 0x37}};
static const GUID IID_ISampleGrabber = {0x6B652FFF, 0x11FE, 0x4fce, {0x92, 0xAD, 0x02, 0x66, 0xB5, 0xD7, 0xC7, 0x8F}};
static const GUID IID_ISampleGrabberCB = {0x0579154A, 0x2B53, 0x4994, {0xB0, 0xD0, 0xE7, 0x73, 0x14, 0x8E, 0xFF, 0x85}};

MIDL_INTERFACE("0579154A-2B53-4994-B0D0-E773148EFF85")
ISampleGrabberCB : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE SampleCB(double SampleTime, IMediaSample *pSample) = 0;
    virtual HRESULT STDMETHODCALLTYPE BufferCB(double SampleTime, BYTE *pBuffer, long BufferLen) = 0;
};

MIDL_INTERFACE("6B652FFF-11FE-4fce-92AD-0266B5D7C78F")
ISampleGrabber : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE SetOneShot(BOOL OneShot) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMediaType(const AM_MEDIA_TYPE *pType) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(AM_MEDIA_TYPE *pType) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(BOOL BufferThem) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(long *pBufferSize, long *pBuffer) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(IMediaSample **ppSample) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCallback(ISampleGrabberCB *pCallback, long WhichMethodToCallback) = 0;
};

namespace cvn {

class SampleGrabberCallback : public ISampleGrabberCB {
private:
    LONG m_refCount = 1;
public:
    unsigned char* dados = nullptr;
    int largura = 0;
    int altura = 0;
    int tamanho = 0;
    bool novo_frame = false;
    CRITICAL_SECTION cs;

    SampleGrabberCallback() { InitializeCriticalSection(&cs); }
    ~SampleGrabberCallback() { DeleteCriticalSection(&cs); if (dados) free(dados); }

    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) {
        if (riid == IID_ISampleGrabberCB || riid == IID_IUnknown) {
            *ppv = static_cast<ISampleGrabberCB*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_refCount); }
    STDMETHODIMP_(ULONG) Release() { 
        LONG ref = InterlockedDecrement(&m_refCount);
        if (ref == 0) delete this;
        return ref;
    }

    STDMETHODIMP SampleCB(double Time, IMediaSample *pSample) { return S_OK; }

    STDMETHODIMP BufferCB(double Time, BYTE *pBuffer, long BufferLen) {
        EnterCriticalSection(&cs);
        if (!dados || tamanho != BufferLen) {
            if (dados) free(dados);
            dados = (unsigned char*)malloc(BufferLen);
            tamanho = BufferLen;
        }
        if (dados) {
            memcpy(dados, pBuffer, BufferLen);
            novo_frame = true;
        }
        LeaveCriticalSection(&cs);
        return S_OK;
    }

    unsigned char* obter_frame_rgba(int& out_largura, int& out_altura) {
        EnterCriticalSection(&cs);
        if (!dados || !novo_frame || largura <= 0 || altura <= 0) {
            LeaveCriticalSection(&cs);
            return nullptr;
        }
        int pixels = largura * altura;
        unsigned char* rgba = (unsigned char*)malloc(pixels * 4);
        if (rgba) {
            for (int y = 0; y < altura; y++) {
                for (int x = 0; x < largura; x++) {
                    int src_y = altura - 1 - y;
                    int src_idx = (src_y * largura + x) * 3;
                    int dst_idx = (y * largura + x) * 4;
                    rgba[dst_idx + 0] = dados[src_idx + 2];
                    rgba[dst_idx + 1] = dados[src_idx + 1];
                    rgba[dst_idx + 2] = dados[src_idx + 0];
                    rgba[dst_idx + 3] = 255;
                }
            }
            out_largura = largura;
            out_altura = altura;
        }
        novo_frame = false;
        LeaveCriticalSection(&cs);
        return rgba;
    }
};

struct Camera {
    IGraphBuilder* graph = nullptr;
    ICaptureGraphBuilder2* capture = nullptr;
    IMediaControl* control = nullptr;
    IBaseFilter* device_filter = nullptr;
    IBaseFilter* grabber_filter = nullptr;
    IBaseFilter* null_renderer = nullptr;
    ISampleGrabber* grabber = nullptr;
    SampleGrabberCallback* callback = nullptr;
    bool ativa = false;
    int largura = 640;
    int altura = 480;
};

class GerenciadorCameras {
private:
    std::vector<Camera*> cameras;
    bool com_inicializado = false;

    void inicializar_com() {
        if (!com_inicializado) {
            CoInitializeEx(NULL, COINIT_MULTITHREADED);
            com_inicializado = true;
        }
    }

    void limpar_camera(Camera* cam) {
        if (!cam) return;
        if (cam->control) { cam->control->Stop(); cam->control->Release(); }
        if (cam->grabber) cam->grabber->Release();
        if (cam->null_renderer) {
            if (cam->graph) cam->graph->RemoveFilter(cam->null_renderer);
            cam->null_renderer->Release();
        }
        if (cam->grabber_filter) {
            if (cam->graph) cam->graph->RemoveFilter(cam->grabber_filter);
            cam->grabber_filter->Release();
        }
        if (cam->device_filter) {
            if (cam->graph) cam->graph->RemoveFilter(cam->device_filter);
            cam->device_filter->Release();
        }
        if (cam->capture) cam->capture->Release();
        if (cam->graph) cam->graph->Release();
        if (cam->callback) delete cam->callback;
        delete cam;
    }

public:
    static GerenciadorCameras& instancia() {
        static GerenciadorCameras inst;
        return inst;
    }

    std::string listar() {
        inicializar_com();
        std::string resultado;
        ICreateDevEnum* dev_enum = nullptr;
        IEnumMoniker* enum_moniker = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                                       IID_ICreateDevEnum, (void**)&dev_enum);
        if (FAILED(hr)) return "";
        hr = dev_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enum_moniker, 0);
        if (hr == S_OK && enum_moniker) {
            IMoniker* moniker = nullptr;
            int index = 0;
            while (enum_moniker->Next(1, &moniker, NULL) == S_OK) {
                IPropertyBag* prop_bag = nullptr;
                hr = moniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&prop_bag);
                if (SUCCEEDED(hr)) {
                    VARIANT var;
                    VariantInit(&var);
                    hr = prop_bag->Read(L"FriendlyName", &var, 0);
                    if (SUCCEEDED(hr)) {
                        int len = WideCharToMultiByte(CP_UTF8, 0, var.bstrVal, -1, NULL, 0, NULL, NULL);
                        char* nome = (char*)malloc(len);
                        WideCharToMultiByte(CP_UTF8, 0, var.bstrVal, -1, nome, len, NULL, NULL);
                        if (!resultado.empty()) resultado += ",";
                        resultado += std::to_string(index) + ":" + nome;
                        free(nome);
                        VariantClear(&var);
                        index++;
                    }
                    prop_bag->Release();
                }
                moniker->Release();
            }
            enum_moniker->Release();
        }
        if (dev_enum) dev_enum->Release();
        return resultado;
    }

    int abrir(int indice) {
        inicializar_com();
        Camera* cam = new Camera();
        HRESULT hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
                                       IID_IGraphBuilder, (void**)&cam->graph);
        if (FAILED(hr)) { delete cam; return -1; }
        hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER,
                               IID_ICaptureGraphBuilder2, (void**)&cam->capture);
        if (FAILED(hr)) { limpar_camera(cam); return -1; }
        cam->capture->SetFiltergraph(cam->graph);
        ICreateDevEnum* dev_enum = nullptr;
        IEnumMoniker* enum_moniker = nullptr;
        hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                               IID_ICreateDevEnum, (void**)&dev_enum);
        if (FAILED(hr)) { limpar_camera(cam); return -1; }
        hr = dev_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enum_moniker, 0);
        if (hr != S_OK || !enum_moniker) {
            dev_enum->Release();
            limpar_camera(cam);
            return -1;
        }
        IMoniker* moniker = nullptr;
        int current = 0;
        bool encontrado = false;
        while (enum_moniker->Next(1, &moniker, NULL) == S_OK) {
            if (current == indice) {
                hr = moniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&cam->device_filter);
                encontrado = SUCCEEDED(hr);
                moniker->Release();
                break;
            }
            moniker->Release();
            current++;
        }
        enum_moniker->Release();
        dev_enum->Release();
        if (!encontrado) { limpar_camera(cam); return -1; }
        cam->graph->AddFilter(cam->device_filter, L"Video Capture");
        hr = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
                               IID_IBaseFilter, (void**)&cam->grabber_filter);
        if (FAILED(hr)) { limpar_camera(cam); return -1; }
        hr = cam->grabber_filter->QueryInterface(IID_ISampleGrabber, (void**)&cam->grabber);
        if (FAILED(hr)) { limpar_camera(cam); return -1; }
        AM_MEDIA_TYPE mt;
        ZeroMemory(&mt, sizeof(mt));
        mt.majortype = MEDIATYPE_Video;
        mt.subtype = MEDIASUBTYPE_RGB24;
        cam->grabber->SetMediaType(&mt);
        cam->graph->AddFilter(cam->grabber_filter, L"Sample Grabber");
        hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER,
                               IID_IBaseFilter, (void**)&cam->null_renderer);
        if (FAILED(hr)) { limpar_camera(cam); return -1; }
        cam->graph->AddFilter(cam->null_renderer, L"Null Renderer");
        hr = cam->capture->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
                                         cam->device_filter, cam->grabber_filter, cam->null_renderer);
        if (FAILED(hr)) { limpar_camera(cam); return -1; }
        AM_MEDIA_TYPE mt_connected;
        hr = cam->grabber->GetConnectedMediaType(&mt_connected);
        if (SUCCEEDED(hr)) {
            if (mt_connected.formattype == FORMAT_VideoInfo) {
                VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)mt_connected.pbFormat;
                cam->largura = vih->bmiHeader.biWidth;
                cam->altura = abs(vih->bmiHeader.biHeight);
            }
            if (mt_connected.pbFormat) CoTaskMemFree(mt_connected.pbFormat);
        }
        cam->callback = new SampleGrabberCallback();
        cam->callback->largura = cam->largura;
        cam->callback->altura = cam->altura;
        cam->grabber->SetCallback(cam->callback, 1);
        cam->grabber->SetBufferSamples(FALSE);
        cam->grabber->SetOneShot(FALSE);
        cam->graph->QueryInterface(IID_IMediaControl, (void**)&cam->control);
        hr = cam->control->Run();
        if (FAILED(hr)) { limpar_camera(cam); return -1; }
        cam->ativa = true;
        int id = (int)cameras.size();
        cameras.push_back(cam);
        return id;
    }

    unsigned char* ler(int id, int& largura, int& altura) {
        if (id < 0 || id >= (int)cameras.size()) return nullptr;
        Camera* cam = cameras[id];
        if (!cam || !cam->ativa || !cam->callback) return nullptr;
        return cam->callback->obter_frame_rgba(largura, altura);
    }

    void fechar(int id) {
        if (id < 0 || id >= (int)cameras.size()) return;
        Camera* cam = cameras[id];
        limpar_camera(cam);
        cameras[id] = nullptr;
    }

    void fechar_todas() {
        for (int i = 0; i < (int)cameras.size(); i++) {
            if (cameras[i]) {
                limpar_camera(cameras[i]);
                cameras[i] = nullptr;
            }
        }
        cameras.clear();
    }

    ~GerenciadorCameras() {
        fechar_todas();
        if (com_inicializado) CoUninitialize();
    }
};

} // namespace cvn

// === PLATAFORMA LINUX (V4L2) ===
#else

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <dirent.h>
#include <pthread.h>

namespace cvn {

struct BufferInfo {
    void* start;
    size_t length;
};

struct Camera {
    int fd = -1;
    BufferInfo* buffers = nullptr;
    int num_buffers = 0;
    int largura = 640;
    int altura = 480;
    bool ativa = false;
    unsigned char* frame_rgba = nullptr;
    pthread_mutex_t mutex;
    bool novo_frame = false;
    pthread_t thread;
    bool thread_ativa = false;
};

class GerenciadorCameras {
private:
    std::vector<Camera*> cameras;

    static void* thread_captura(void* arg) {
        Camera* cam = (Camera*)arg;
        while (cam->thread_ativa) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(cam->fd, &fds);
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 100000;
            int r = select(cam->fd + 1, &fds, NULL, NULL, &tv);
            if (r <= 0) continue;
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            if (ioctl(cam->fd, VIDIOC_DQBUF, &buf) < 0) continue;
            pthread_mutex_lock(&cam->mutex);
            unsigned char* src = (unsigned char*)cam->buffers[buf.index].start;
            if (!cam->frame_rgba) {
                cam->frame_rgba = (unsigned char*)malloc(cam->largura * cam->altura * 4);
            }
            if (cam->frame_rgba) {
                for (int i = 0; i < cam->largura * cam->altura / 2; i++) {
                    int y0 = src[i * 4 + 0];
                    int u  = src[i * 4 + 1];
                    int y1 = src[i * 4 + 2];
                    int v  = src[i * 4 + 3];
                    int c = y0 - 16;
                    int d = u - 128;
                    int e = v - 128;
                    int r0 = (298 * c + 409 * e + 128) >> 8;
                    int g0 = (298 * c - 100 * d - 208 * e + 128) >> 8;
                    int b0 = (298 * c + 516 * d + 128) >> 8;
                    c = y1 - 16;
                    int r1 = (298 * c + 409 * e + 128) >> 8;
                    int g1 = (298 * c - 100 * d - 208 * e + 128) >> 8;
                    int b1 = (298 * c + 516 * d + 128) >> 8;
                    int idx = i * 8;
                    cam->frame_rgba[idx + 0] = r0 < 0 ? 0 : (r0 > 255 ? 255 : r0);
                    cam->frame_rgba[idx + 1] = g0 < 0 ? 0 : (g0 > 255 ? 255 : g0);
                    cam->frame_rgba[idx + 2] = b0 < 0 ? 0 : (b0 > 255 ? 255 : b0);
                    cam->frame_rgba[idx + 3] = 255;
                    cam->frame_rgba[idx + 4] = r1 < 0 ? 0 : (r1 > 255 ? 255 : r1);
                    cam->frame_rgba[idx + 5] = g1 < 0 ? 0 : (g1 > 255 ? 255 : g1);
                    cam->frame_rgba[idx + 6] = b1 < 0 ? 0 : (b1 > 255 ? 255 : b1);
                    cam->frame_rgba[idx + 7] = 255;
                }
                cam->novo_frame = true;
            }
            pthread_mutex_unlock(&cam->mutex);
            ioctl(cam->fd, VIDIOC_QBUF, &buf);
        }
        return nullptr;
    }

    void limpar_camera(Camera* cam) {
        if (!cam) return;
        cam->thread_ativa = false;
        if (cam->thread) pthread_join(cam->thread, nullptr);
        if (cam->fd >= 0) {
            enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            ioctl(cam->fd, VIDIOC_STREAMOFF, &type);
            for (int i = 0; i < cam->num_buffers; i++) {
                if (cam->buffers[i].start) {
                    munmap(cam->buffers[i].start, cam->buffers[i].length);
                }
            }
            close(cam->fd);
        }
        if (cam->buffers) free(cam->buffers);
        if (cam->frame_rgba) free(cam->frame_rgba);
        pthread_mutex_destroy(&cam->mutex);
        delete cam;
    }

public:
    static GerenciadorCameras& instancia() {
        static GerenciadorCameras inst;
        return inst;
    }

    std::string listar() {
        std::string resultado;
        DIR* dir = opendir("/dev");
        if (!dir) return "";
        struct dirent* entry;
        int index = 0;
        while ((entry = readdir(dir)) != nullptr) {
            if (strncmp(entry->d_name, "video", 5) == 0) {
                std::string path = "/dev/" + std::string(entry->d_name);
                int fd = open(path.c_str(), O_RDWR);
                if (fd >= 0) {
                    struct v4l2_capability cap;
                    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) >= 0) {
                        if (cap.device_caps & V4L2_CAP_VIDEO_CAPTURE) {
                            if (!resultado.empty()) resultado += ",";
                            resultado += std::to_string(index) + ":" + (char*)cap.card;
                            index++;
                        }
                    }
                    close(fd);
                }
            }
        }
        closedir(dir);
        return resultado;
    }

    int abrir(int indice) {
        DIR* dir = opendir("/dev");
        if (!dir) return -1;
        struct dirent* entry;
        int current = 0;
        std::string device_path;
        while ((entry = readdir(dir)) != nullptr) {
            if (strncmp(entry->d_name, "video", 5) == 0) {
                std::string path = "/dev/" + std::string(entry->d_name);
                int fd = open(path.c_str(), O_RDWR);
                if (fd >= 0) {
                    struct v4l2_capability cap;
                    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) >= 0) {
                        if (cap.device_caps & V4L2_CAP_VIDEO_CAPTURE) {
                            if (current == indice) {
                                device_path = path;
                                close(fd);
                                break;
                            }
                            current++;
                        }
                    }
                    close(fd);
                }
            }
        }
        closedir(dir);
        if (device_path.empty()) return -1;

        Camera* cam = new Camera();
        pthread_mutex_init(&cam->mutex, nullptr);
        cam->fd = open(device_path.c_str(), O_RDWR);
        if (cam->fd < 0) { limpar_camera(cam); return -1; }

        struct v4l2_format fmt;
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = 640;
        fmt.fmt.pix.height = 480;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
        if (ioctl(cam->fd, VIDIOC_S_FMT, &fmt) < 0) { limpar_camera(cam); return -1; }
        cam->largura = fmt.fmt.pix.width;
        cam->altura = fmt.fmt.pix.height;

        struct v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));
        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        if (ioctl(cam->fd, VIDIOC_REQBUFS, &req) < 0) { limpar_camera(cam); return -1; }

        cam->buffers = (BufferInfo*)calloc(req.count, sizeof(BufferInfo));
        cam->num_buffers = req.count;

        for (int i = 0; i < (int)req.count; i++) {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            if (ioctl(cam->fd, VIDIOC_QUERYBUF, &buf) < 0) { limpar_camera(cam); return -1; }
            cam->buffers[i].length = buf.length;
            cam->buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                          MAP_SHARED, cam->fd, buf.m.offset);
            if (cam->buffers[i].start == MAP_FAILED) { limpar_camera(cam); return -1; }
        }

        for (int i = 0; i < cam->num_buffers; i++) {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            ioctl(cam->fd, VIDIOC_QBUF, &buf);
        }

        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(cam->fd, VIDIOC_STREAMON, &type) < 0) { limpar_camera(cam); return -1; }

        cam->ativa = true;
        cam->thread_ativa = true;
        pthread_create(&cam->thread, nullptr, thread_captura, cam);

        int id = (int)cameras.size();
        cameras.push_back(cam);
        return id;
    }

    unsigned char* ler(int id, int& largura, int& altura) {
        if (id < 0 || id >= (int)cameras.size()) return nullptr;
        Camera* cam = cameras[id];
        if (!cam || !cam->ativa) return nullptr;

        pthread_mutex_lock(&cam->mutex);
        unsigned char* resultado = nullptr;
        if (cam->novo_frame && cam->frame_rgba) {
            int tamanho = cam->largura * cam->altura * 4;
            resultado = (unsigned char*)malloc(tamanho);
            if (resultado) {
                memcpy(resultado, cam->frame_rgba, tamanho);
                largura = cam->largura;
                altura = cam->altura;
            }
            cam->novo_frame = false;
        }
        pthread_mutex_unlock(&cam->mutex);
        return resultado;
    }

    void fechar(int id) {
        if (id < 0 || id >= (int)cameras.size()) return;
        Camera* cam = cameras[id];
        limpar_camera(cam);
        cameras[id] = nullptr;
    }

    void fechar_todas() {
        for (int i = 0; i < (int)cameras.size(); i++) {
            if (cameras[i]) {
                limpar_camera(cameras[i]);
                cameras[i] = nullptr;
            }
        }
        cameras.clear();
    }

    ~GerenciadorCameras() { fechar_todas(); }
};

} // namespace cvn

#endif // _WIN32

#endif // CVN_CAMERA_HPP