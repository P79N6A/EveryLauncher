#include "detailedtext.h"

DetailedW::DetailedW(QWidget *parent) : QWidget(parent)
{

}

void DetailedW::showDoc(Rcl::Doc doc)
{

}

void DetailedW::setHighlightData(HighlightData hl)
{
    this->m_hData=hl;
}
