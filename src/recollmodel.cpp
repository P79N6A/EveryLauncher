#include "recollmodel.h"

#include <bits/stl_list.h>
#include <bits/stl_map.h>

#include <QDebug>
#include <QIcon>
#include <QMessageBox>
#include <QPixmap>
#include <plaintorich.h>

class PlainToRichQtReslist : public PlainToRich {
public:
    ~PlainToRichQtReslist() override = default;

    string startMatch(unsigned int) override {
        return string("<span style='") + "TODO" + string("'>");
    }

    string endMatch() override { return string("</span>"); }
};

static PlainToRichQtReslist g_hiliter;

static QString gengetter(const string &fld, const Rcl::Doc &doc) {
    const auto it = doc.meta.find(fld);
    if (it == doc.meta.end()) {
        return "";
    }
    return QString::fromStdString(it->second);
}

static QString sizegetter(const string &fld, const Rcl::Doc &doc) {
    const auto it = doc.meta.find(fld);
    if (it == doc.meta.end()) {
        return "";
    }
    int64_t size = atoll(it->second.c_str());
    return QString::fromStdString(displayableBytes(size) + " (" + it->second +
                                  ")");
}

static QString dategetter(const string &, const Rcl::Doc &doc) {
    string sdate;
    if (!doc.dmtime.empty() || !doc.fmtime.empty()) {
        char datebuf[100];
        datebuf[0] = 0;
        time_t mtime = doc.dmtime.empty() ? atoll(doc.fmtime.c_str())
                                          : atoll(doc.dmtime.c_str());
        struct tm *tm = localtime(&mtime);
        strftime(datebuf, 99, "%Y-%m-%d", tm);
        //        transcode(datebuf, sdate, "", "UTF-8");
    }
    return QString::fromStdString(sdate);
}

static QString datetimegetter(const string &, const Rcl::Doc &doc) {
    char datebuf[100];
    datebuf[0] = 0;
    if (!doc.dmtime.empty() || !doc.fmtime.empty()) {
        time_t mtime = doc.dmtime.empty() ? atoll(doc.fmtime.c_str())
                                          : atoll(doc.dmtime.c_str());
        struct tm *tm = localtime(&mtime);
        //    strftime(datebuf, 99, prefs.creslistdateformat.c_str(), tm);
    }
    return datebuf;
}

map<string, QString> RecollModel::o_displayableFields;

FieldGetter *RecollModel::chooseGetter(const string &field) {
    if (!stringlowercmp("date", field))
        return dategetter;
    else if (!stringlowercmp("datetime", field))
        return datetimegetter;
    else if (!stringlowercmp("bytes", field.substr(1)))
        return sizegetter;
    else
        return gengetter;
}

RecollModel::RecollModel(const QStringList fields, QObject *parent)
        : QAbstractListModel(parent) {
    // Initialize the translated map for column headers
    o_displayableFields["abstract"] = tr("Abstract");
    o_displayableFields["author"] = tr("Author");
    o_displayableFields["dbytes"] = tr("Document size");
    o_displayableFields["dmtime"] = tr("Document date");
    o_displayableFields["fbytes"] = tr("File size");
    o_displayableFields["filename"] = tr("File name");
    o_displayableFields["fmtime"] = tr("File date");
    o_displayableFields["ipath"] = tr("Ipath");
    o_displayableFields["keywords"] = tr("Keywords");
    o_displayableFields["mtype"] = tr("MIME type");
    o_displayableFields["origcharset"] = tr("Original character set");
    o_displayableFields["relevancyrating"] = tr("Relevancy rating");
    o_displayableFields["title"] = tr("Title");
    o_displayableFields["url"] = tr("URL");
    o_displayableFields["mtime"] = tr("Mtime");
    o_displayableFields["date"] = tr("Date");
    o_displayableFields["datetime"] = tr("Date and time");

    if (theconfig) {
        const set<string> &stored = theconfig->getStoredFields();
        for (const auto &it : stored) {
            if (o_displayableFields.find(it) == o_displayableFields.end()) {
                o_displayableFields[it] = QString::fromUtf8(it.c_str());
            }
        }
    }

    // Construct the actual list of column names
    for (const auto &field : fields) {
        m_fields.emplace_back((const char *) (field.toUtf8()));
        m_getters.push_back(chooseGetter(m_fields.back()));
    }

    g_hiliter.set_inputhtml(false);
}


int RecollModel::rowCount(const QModelIndex &) const {
    if (!m_source)
        return 0;
    auto cnt = m_source->getResCnt();
    return cnt;
}

void RecollModel::readDocSource() {
    beginResetModel();
    endResetModel();
}

void RecollModel::setDocSource(std::shared_ptr<DocSequence> nsource) {
    if (!nsource) {
        m_source = std::shared_ptr<DocSequence>();
    } else {

        m_source = std::shared_ptr<DocSequence>(new DocSource(theconfig, nsource));
        m_hdata.clear();
        m_source->getTerms(m_hdata);
    }
}

QVariant RecollModel::headerData(int idx, Qt::Orientation orientation,
                                 int role) const {
    if (orientation == Qt::Vertical && role == Qt::DisplayRole) {
        return idx;
    }
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole &&
        idx < int(m_fields.size())) {
        map<string, QString>::const_iterator it =
                o_displayableFields.find(m_fields[idx]);
        if (it == o_displayableFields.end())
            return QString::fromUtf8(m_fields[idx].c_str());
        else
            return it->second;
    }
    return QVariant();
}

QVariant RecollModel::data(const QModelIndex &index, int role) const {
    if (!m_source || !index.isValid() || role < Qt::UserRole) {
        return QVariant();
    }
    Rcl::Doc doc;
    std::list<std::string> lr;
    if (!m_source->getDoc(index.row(), doc)) {
        return QVariant();
    }
    QVariant var;
    switch (role) {
        case Role_FILE_NAME: {
            var = gengetter("filename", doc);
            break;
        }
        case Role_LOCATION: {
            var = gengetter("url", doc);
            break;
        }
        case Role_FILE_SIMPLE_CONTENT: {
            var = gengetter("abstract", doc);
            g_hiliter.plaintorich(var.toString().toStdString(), lr, m_hdata);
            var = QString::fromUtf8(lr.front().c_str());
            break;
        }
        case Role_MIME_TYPE: {
            var = gengetter("mtype", doc);
            break;
        }
        case Role_RELEVANCY: {
            var = gengetter("relevancyrating", doc);
            break;
        }
            // the default cat icon
        case Role_ICON_PATH: {
            auto mtype = gengetter("mtype", doc);
            if (mtype != "application/x-all") {
                std::string apptag;
                doc.getmeta(Rcl::Doc::keyapptg, &apptag);
                var = QString::fromStdString(
                        theconfig->getMimeIconPath(doc.mimetype, apptag));
            } else {
                var = gengetter("appicon", doc);
            }
            break;
        }
        case Role_APP_NAME: {
            var = gengetter("appname", doc);
            break;
        }
        case Role_APP_COMMENT: {
            var = gengetter("appcomment", doc);
            break;
        }
        case Role_NODISPLAY: {
            var = gengetter("appnodisplay", doc);
            break;
        }
        default:
            break;
    }
    return var;
}
