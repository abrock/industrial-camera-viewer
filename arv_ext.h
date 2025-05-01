#ifndef ARVEXT_H
#define ARVEXT_H

#include <arv.h>

namespace ArvExt {

bool isBayer(const ArvPixelFormat fmt);

int demosaicingVNG(const ArvPixelFormat fmt);

}  // namespace ArvExt

#endif  // ARVEXT_H
