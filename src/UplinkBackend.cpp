#include "UplinkBackend.h"

UplinkBackend::UplinkBackend(QObject* parent)
    : UplinkUiSimpleSource(parent)
{
    setReady(false);
}

void UplinkBackend::onContextReady()
{
    setReady(true);
}
