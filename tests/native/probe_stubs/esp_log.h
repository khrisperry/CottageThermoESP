#pragma once
void probe_test_log(const char *tag,const char *format,...);
#define ESP_LOGI(...) probe_test_log(__VA_ARGS__)
#define ESP_LOGW(...) probe_test_log(__VA_ARGS__)
