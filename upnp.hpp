#pragma once
#include <iostream>
#include <string>
#include <windows.h>
#include <comdef.h>
#include <natupnp.h>

// Автоматически подключаем библиотеки Windows для работы с COM-интерфейсом
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

class RouterManager {
public:
    // Функция проброса порта
    static bool forwardPort(unsigned short port, const std::string& localIp, const std::string& protocol = "TCP") {
        IUPnPNAT* nat = nullptr;
        IStaticPortMappingCollection* mappingCollection = nullptr;
        IStaticPortMapping* mapping = nullptr;
        bool success = false;

        // Инициализируем COM-интерфейс Windows
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

        // Создаем объект для работы с UPnP роутера
        hr = CoCreateInstance(__uuidof(UPnPNAT), NULL, CLSCTX_ALL, __uuidof(IUPnPNAT), (void**)&nat);
        if (SUCCEEDED(hr) && nat != nullptr) {
            hr = nat->get_StaticPortMappingCollection(&mappingCollection);
            if (SUCCEEDED(hr) && mappingCollection != nullptr) {
                
                // Подготавливаем данные для роутера
                _bstr_t bstrProtocol(protocol.c_str());
                _bstr_t bstrInternalClient(localIp.c_str());
                _bstr_t bstrDescription("Messenger Chat Relay");

                // Даем роутеру команду добавить правило
                hr = mappingCollection->Add(port, bstrProtocol, port, bstrInternalClient, VARIANT_TRUE, bstrDescription, &mapping);
                
                if (SUCCEEDED(hr) && mapping != nullptr) {
                    success = true; // Порт успешно открыт!
                    mapping->Release();
                }
                mappingCollection->Release();
            }
            nat->Release();
        }
        
        CoUninitialize();
        return success;
    }
};