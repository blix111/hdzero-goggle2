#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// On prévient le compilateur que la "boîte" page_pack_t existe quelque part, 
// pas besoin de chercher sa taille tout de suite !
typedef struct page_pack page_pack_t;

extern page_pack_t pp_analog_scanner;

#ifdef __cplusplus
}
#endif
