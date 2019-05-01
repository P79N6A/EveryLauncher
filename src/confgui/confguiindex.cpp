/* Copyright (C) 2007 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <qglobal.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFrame>
#include <qlayout.h>
#include <qwidget.h>
#include <qlabel.h>
#include <qtimer.h>
#include <qmessagebox.h>
#include <qcheckbox.h>
#include <QListWidget>

#include <list>
#include <set>
#include <string>
#include <functional>
using std::list;
using std::set;
using std::string;

#include "confgui.h"
//#include "recoll.h"
#include "confguiindex.h"
#include "smallut.h"
#include "log.h"
#include "rcldb.h"
#include "execmd.h"
#include "rclconfig.h"

namespace confgui {
static const int spacing = 3;
static const int margin = 3;

/** 
 * A Gui-to-Data link class for ConfTree
 * Has a subkey pointer member which makes it easy to change the
 * current subkey for a number at a time.
 */
class ConfLinkRclRep : public ConfLinkRep {
public:
    ConfLinkRclRep(ConfNull *conf, const string& nm, 
		   string *sk = 0)
	: m_conf(conf), m_nm(nm), m_sk(sk) /* KEEP THE POINTER, shared data */
    {
    }
    virtual ~ConfLinkRclRep() {}

    virtual bool set(const string& val) 
    {
	if (!m_conf)
	    return false;
	LOGDEB1("Setting [" << m_nm << "] value to ["  << val << "]\n");
	bool ret = m_conf->set(m_nm, val, getSk());
	if (!ret)
	    LOGERR("Value set failed\n" );
	return ret;
    }
    virtual bool get(string& val) 
    {
	if (!m_conf)
	    return false;
	bool ret = m_conf->get(m_nm, val, getSk());
	LOGDEB1("ConfLinkRcl::get: [" << m_nm << "] sk [" <<
                getSk() << "] -> ["  << (ret ? val : "no value") << "]\n" );
	return ret;
    }
private:
    string getSk() {
        return m_sk ? *m_sk : string();
    }
    ConfNull     *m_conf;
    const string  m_nm;
    const string *m_sk;
};


typedef std::function<vector<string>()> RclConfVecValueGetter;

/* Special link for skippedNames and noContentSuffixes which are
   computed as set differences */
class ConfLinkPlusMinus : public ConfLinkRep {
public:
    ConfLinkPlusMinus(RclConfig *rclconf, ConfNull *conf,
                 const string& basename, RclConfVecValueGetter getter,
                 string *sk = 0)
	: m_rclconf(rclconf), m_conf(conf),
          m_basename(basename), m_getter(getter),
          m_sk(sk) /* KEEP THE POINTER, shared data */
    {
    }
    virtual ~ConfLinkPlusMinus() {}

    virtual bool set(const string& snval) {
	if (!m_conf || !m_rclconf)
	    return false;

        string sbase;
        m_conf->get(m_basename, sbase, getSk());
        std::set<string> nval;
        stringToStrings(snval, nval);
        string splus, sminus;
        RclConfig::setPlusMinus(sbase, nval, splus, sminus);
        LOGDEB1("ConfLinkPlusMinus: base [" << sbase << "] nvalue [" << snval <<
                "] splus [" << splus << "] sminus [" << sminus << "]\n");
        if (!m_conf->set(m_basename + "-", sminus, getSk())) {
            return false;
        }
        if (!m_conf->set(m_basename + "+", splus, getSk())) {
            return false;
        }
        return true;
    }

    virtual bool get(string& val) {
	if (!m_conf || !m_rclconf)
	    return false;

        m_rclconf->setKeyDir(getSk());
        vector<string> vval = m_getter();
        val = stringsToString(vval);
        LOGDEB1("ConfLinkPlusMinus: "<<m_basename<<" -> " << val <<std::endl);
	return true;
    }

private:
    string getSk() {
        return m_sk ? *m_sk : string();
    }
      
    RclConfig    *m_rclconf;
    ConfNull     *m_conf;
    string        m_basename;
    RclConfVecValueGetter m_getter;
    const string *m_sk;
};


ConfIndexW::ConfIndexW(QWidget *parent, RclConfig *config)
    : QDialog(parent), m_rclconf(config)
{
    QString title("Recoll - Index Settings : ");
    title += QString::fromLocal8Bit(config->getConfDir().c_str());
    setWindowTitle(title);
    tabWidget = new QTabWidget;
    reloadPanels();

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok
				     | QDialogButtonBox::Cancel);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(tabWidget);
    mainLayout->addWidget(buttonBox);
    setLayout(mainLayout);

    resize(QSize(600, 450).expandedTo(minimumSizeHint()));

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(acceptChanges()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(rejectChanges()));
}

void ConfIndexW::acceptChanges()
{
    LOGDEB("ConfIndexW::acceptChanges()\n" );
    if (!m_conf) {
	LOGERR("ConfIndexW::acceptChanges: no config\n" );
	return;
    }
    // Let the changes to disk
    if (!m_conf->holdWrites(false)) {
	QMessageBox::critical(0, "Recoll",  
			      tr("Can't write configuration file"));
    }
    // Delete local copy and update the main one from the file
    delete m_conf;
    m_conf = 0;
    m_rclconf->updateMainConfig();

    hide();
}

void ConfIndexW::rejectChanges()
{
    LOGDEB("ConfIndexW::rejectChanges()\n" );
    // Discard local changes.
    delete m_conf;
    m_conf = 0;
    hide();
}

void ConfIndexW::reloadPanels()
{
    if ((m_conf = m_rclconf->cloneMainConfig()) == 0) 
	return;
    m_conf->holdWrites(true);
    tabWidget->clear();
    m_widgets.clear();

    QWidget *w = new ConfTopPanelW(this, m_conf);
    m_widgets.push_back(w);
    tabWidget->addTab(w, QObject::tr("Global parameters"));
	
    w = new ConfSubPanelW(this, m_conf, m_rclconf);
    m_widgets.push_back(w);
    tabWidget->addTab(w, QObject::tr("Local parameters"));

}


ConfTopPanelW::ConfTopPanelW(QWidget *parent, ConfNull *config)
    : QWidget(parent)
{
    QWidget *w = 0;

    QGridLayout *gl1 = new QGridLayout(this);
    gl1->setSpacing(spacing);
    gl1->setMargin(margin);

    int gridrow = 0;

    w = new ConfParamDNLW(this, 
                          ConfLink(new ConfLinkRclRep(config, "topdirs")), 
                          tr("Top directories"),
                          tr("The list of directories where recursive "
                             "indexing starts. Default: your home."));
    setSzPol(w, QSizePolicy::Preferred, QSizePolicy::Preferred, 1, 3);
    gl1->addWidget(w, gridrow++, 0, 1, 2);

    ConfParamSLW *eskp = new 
	ConfParamSLW(this, 
                     ConfLink(new ConfLinkRclRep(config, "skippedPaths")), 
                     tr("Skipped paths"),
		     tr("These are pathnames of directories which indexing "
			"will not enter.<br>"
                        "Path elements may contain wildcards. "
			"The entries must match the paths seen by the indexer "
                        "(e.g.: if topdirs includes '/home/me' and '/home' is "
                        "actually a link "
			"to '/usr/home', a correct skippedPath entry "
			"would be '/home/me/tmp*', not '/usr/home/me/tmp*')"));
    eskp->setFsEncoding(true);
    setSzPol(eskp, QSizePolicy::Preferred, QSizePolicy::Preferred, 1, 3);
    gl1->addWidget(eskp, gridrow++, 0, 1, 2);

    vector<string> cstemlangs = Rcl::Db::getStemmerNames();
    QStringList stemlangs;
    for (vector<string>::const_iterator it = cstemlangs.begin(); 
	 it != cstemlangs.end(); it++) {
	stemlangs.push_back(QString::fromUtf8(it->c_str()));
    }
    w = new 
	ConfParamCSLW(this, 
                      ConfLink(new ConfLinkRclRep(config, 
                                                  "indexstemminglanguages")),
                      tr("Stemming languages"),
		      tr("The languages for which stemming expansion<br>"
			 "dictionaries will be built."), stemlangs);
    setSzPol(w, QSizePolicy::Preferred, QSizePolicy::Preferred, 1, 1);
    gl1->addWidget(w, gridrow, 0);
    

    w = new ConfParamFNW(this, 
                         ConfLink(new ConfLinkRclRep(config, "logfilename")), 
                         tr("Log file name"),
                         tr("The file where the messages will be written.<br>"
                            "Use 'stderr' for terminal output"), false);
    gl1->addWidget(w, gridrow++, 1);

    
    w = new ConfParamIntW(this, 
                          ConfLink(new ConfLinkRclRep(config, "loglevel")), 
                          tr("Log verbosity level"),
                          tr("This value adjusts the amount of "
                             "messages,<br>from only errors to a "
                             "lot of debugging data."), 0, 6);
    gl1->addWidget(w, gridrow, 0);

    w = new ConfParamIntW(this, 
                          ConfLink(new ConfLinkRclRep(config, "idxflushmb")),
                          tr("Index flush megabytes interval"),
                          tr("This value adjust the amount of "
			 "data which is indexed between flushes to disk.<br>"
			 "This helps control the indexer memory usage. "
			 "Default 10MB "), 0, 1000);
    gl1->addWidget(w, gridrow++, 1);

    w = new ConfParamIntW(this, 
                          ConfLink(new ConfLinkRclRep(config, "maxfsoccuppc")),
                          tr("Max disk occupation (%, 0 means no limit)"),
                          tr("This is the percentage of disk usage "
                             "- total disk usage, not index size - at which "
                             "indexing will fail and stop.<br>"
                             "The default value of 0 removes any limit."),
                          0, 100);
    gl1->addWidget(w, gridrow++, 0);

    ConfParamBoolW* cpasp =
        new ConfParamBoolW(this, 
                           ConfLink(new ConfLinkRclRep(config, "noaspell")), 
                           tr("No aspell usage"),
                           tr("Disables use of aspell to generate spelling "
                              "approximation in the term explorer tool.<br> "
                              "Useful if aspell is absent or does not work. "));
    gl1->addWidget(cpasp, gridrow, 0);

    ConfParamStrW* cpaspl = new 
        ConfParamStrW(this, 
                      ConfLink(new ConfLinkRclRep(config, "aspellLanguage")),
                      tr("Aspell language"),
                      tr("The language for the aspell dictionary. "
                         "This should look like 'en' or 'fr' ...<br>"
                         "If this value is not set, the NLS environment "
			 "will be used to compute it, which usually works. "
			 "To get an idea of what is installed on your system, "
			 "type 'aspell config' and look for .dat files inside "
			 "the 'data-dir' directory. "));
    cpaspl->setEnabled(!cpasp->m_cb->isChecked());
    connect(cpasp->m_cb, SIGNAL(toggled(bool)), cpaspl,SLOT(setDisabled(bool)));
    gl1->addWidget(cpaspl, gridrow++, 1);

    w = new 
        ConfParamFNW(this, 
                     ConfLink(new ConfLinkRclRep(config, "dbdir")),
                     tr("Database directory name"),
                     tr("The name for a directory where to store the index<br>"
			"A non-absolute path is taken relative to the "
			"configuration directory. The default is 'xapiandb'."
			), true);
    gl1->addWidget(w, gridrow++, 0, 1, 2);
    
    w = new 
	ConfParamStrW(this, 
                      ConfLink(new ConfLinkRclRep(config, "unac_except_trans")),
                      tr("Unac exceptions"),
                      tr("<p>These are exceptions to the unac mechanism "
                         "which, by default, removes all diacritics, "
                         "and performs canonic decomposition. You can override "
                         "unaccenting for some characters, depending on your "
                         "language, and specify additional decompositions, "
                         "e.g. for ligatures. In each space-separated entry, "
                         "the first character is the source one, and the rest "
                         "is the translation."
                          ));
    gl1->addWidget(w, gridrow++, 0, 1, 2);
}

ConfSubPanelW::ConfSubPanelW(QWidget *parent, ConfNull *config, 
                             RclConfig *rclconf)
    : QWidget(parent), m_config(config)
{
    QVBoxLayout *vboxLayout = new QVBoxLayout(this);
    vboxLayout->setSpacing(spacing);
    vboxLayout->setMargin(margin);

    m_subdirs = new 
	ConfParamDNLW(this, 
                      ConfLink(new ConfLinkNullRep()), 
		      QObject::tr("<b>Customised subtrees"),
		      QObject::tr("The list of subdirectories in the indexed "
				  "hierarchy <br>where some parameters need "
				  "to be redefined. Default: empty."));
    m_subdirs->getListBox()->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_subdirs->getListBox(), 
	    SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
	    this, 
	    SLOT(subDirChanged(QListWidgetItem *, QListWidgetItem *)));
    connect(m_subdirs, SIGNAL(entryDeleted(QString)),
	    this, SLOT(subDirDeleted(QString)));

    // We only retrieve the subkeys from the user's config (shallow),
    // no use to confuse the user by showing the subtrees which are
    // customized in the system config like .thunderbird or
    // .purple. This doesn't prevent them to add and customize them
    // further.
    vector<string> allkeydirs = config->getSubKeys(true); 

    QStringList qls;
    for (vector<string>::const_iterator it = allkeydirs.begin(); 
	 it != allkeydirs.end(); it++) {
	qls.push_back(QString::fromUtf8(it->c_str()));
    }
    m_subdirs->getListBox()->insertItems(0, qls);
    vboxLayout->addWidget(m_subdirs);

    QFrame *line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    vboxLayout->addWidget(line2);

    QLabel *explain = new QLabel(this);
    explain->setText(
		     QObject::
		     tr("<i>The parameters that follow are set either at the "
			"top level, if nothing<br>"
			"or an empty line is selected in the listbox above, "
			"or for the selected subdirectory.<br>"
			"You can add or remove directories by clicking "
			"the +/- buttons."));
    vboxLayout->addWidget(explain);


    m_groupbox = new QGroupBox(this);
    setSzPol(m_groupbox, QSizePolicy::Preferred, QSizePolicy::Preferred, 1, 3);

    QGridLayout *gl1 = new QGridLayout(m_groupbox);
    gl1->setSpacing(spacing);
    gl1->setMargin(margin);
    int gridy = 0;

    ConfParamSLW *eskn = new ConfParamSLW(
        m_groupbox, 
        ConfLink(new ConfLinkPlusMinus(
                     rclconf, config, "skippedNames",
                     std::bind(&RclConfig::getSkippedNames, rclconf), &m_sk)),
        QObject::tr("Skipped names"),
        QObject::tr("These are patterns for file or directory "
                    " names which should not be indexed."));
    eskn->setFsEncoding(true);
    m_widgets.push_back(eskn);
    gl1->addWidget(eskn, gridy, 0);

    vector<string> amimes = rclconf->getAllMimeTypes();
    QStringList amimesq;
    for (vector<string>::const_iterator it = amimes.begin(); 
	 it != amimes.end(); it++) {
	amimesq.push_back(QString::fromUtf8(it->c_str()));
    }

    ConfParamCSLW *eincm = new ConfParamCSLW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "indexedmimetypes", &m_sk)),
        tr("Only mime types"),
        tr("An exclusive list of indexed mime types.<br>Nothing "
           "else will be indexed. Normally empty and inactive"), amimesq);
    m_widgets.push_back(eincm);
    gl1->addWidget(eincm, gridy++, 1);

    ConfParamCSLW *eexcm = new ConfParamCSLW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "excludedmimetypes", &m_sk)),
        tr("Exclude mime types"),
        tr("Mime types not to be indexed"), amimesq);
    m_widgets.push_back(eexcm);
    gl1->addWidget(eexcm, gridy, 0);

    ConfParamSLW *encs = new ConfParamSLW(
        m_groupbox, 
        ConfLink(new ConfLinkPlusMinus(
                     rclconf, config, "noContentSuffixes",
                     std::bind(&RclConfig::getStopSuffixes, rclconf), &m_sk)),
        QObject::tr("Ignored endings"),
        QObject::tr("These are file name endings for files which will be "
                    "indexed by name only \n(no MIME type identification "
                    "attempt, no decompression, no content indexing)."));
    encs->setFsEncoding(true);
    m_widgets.push_back(encs);
    gl1->addWidget(encs, gridy++, 1);

    vector<string> args;
    args.push_back("-l");
    ExecCmd ex;
    string icout;
    string cmd = "iconv";
    int status = ex.doexec(cmd, args, 0, &icout);
    if (status) {
	LOGERR("Can't get list of charsets from 'iconv -l'" );
    }
    icout = neutchars(icout, ",");
    list<string> ccsets;
    stringToStrings(icout, ccsets);
    QStringList charsets;
    charsets.push_back("");
    for (list<string>::const_iterator it = ccsets.begin(); 
	 it != ccsets.end(); it++) {
	charsets.push_back(QString::fromUtf8(it->c_str()));
    }

    ConfParamCStrW *e21 = new ConfParamCStrW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "defaultcharset", &m_sk)), 
        QObject::tr("Default<br>character set"),
        QObject::tr("Character set used for reading files "
                    "which do not identify the character set "
                    "internally, for example pure text files.<br>"
                    "The default value is empty, "
                    "and the value from the NLS environnement is used."
            ), charsets);
    m_widgets.push_back(e21);
    gl1->addWidget(e21, gridy++, 0);

    ConfParamBoolW *e3 = new ConfParamBoolW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "followLinks", &m_sk)), 
        QObject::tr("Follow symbolic links"),
        QObject::tr("Follow symbolic links while "
                    "indexing. The default is no, "
                    "to avoid duplicate indexing"));
    m_widgets.push_back(e3);
    gl1->addWidget(e3, gridy, 0);

    ConfParamBoolW *eafln = new ConfParamBoolW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "indexallfilenames", &m_sk)), 
        QObject::tr("Index all file names"),
        QObject::tr("Index the names of files for which the contents "
                    "cannot be identified or processed (no or "
                    "unsupported mime type). Default true"));
    m_widgets.push_back(eafln);
    gl1->addWidget(eafln, gridy++, 1);

    ConfParamIntW *ezfmaxkbs = new ConfParamIntW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "compressedfilemaxkbs", &m_sk)), 
        tr("Max. compressed file size (KB)"),
        tr("This value sets a threshold beyond which compressed"
           "files will not be processed. Set to -1 for no "
           "limit, to 0 for no decompression ever."), -1, 1000000, -1);
    m_widgets.push_back(ezfmaxkbs);
    gl1->addWidget(ezfmaxkbs, gridy, 0);

    ConfParamIntW *etxtmaxmbs = new ConfParamIntW(
        m_groupbox,
        ConfLink(new ConfLinkRclRep(config, "textfilemaxmbs", &m_sk)), 
        tr("Max. text file size (MB)"),
        tr("This value sets a threshold beyond which text "
           "files will not be processed. Set to -1 for no "
           "limit. \nThis is for excluding monster "
           "log files from the index."), -1, 1000000);
    m_widgets.push_back(etxtmaxmbs);
    gl1->addWidget(etxtmaxmbs, gridy++, 1);

    ConfParamIntW *etxtpagekbs = new ConfParamIntW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "textfilepagekbs", &m_sk)),
        tr("Text file page size (KB)"),
        tr("If this value is set (not equal to -1), text "
           "files will be split in chunks of this size for "
           "indexing.\nThis will help searching very big text "
           " files (ie: log files)."), -1, 1000000);
    m_widgets.push_back(etxtpagekbs);
    gl1->addWidget(etxtpagekbs, gridy, 0);

    ConfParamIntW *efiltmaxsecs = new ConfParamIntW(
        m_groupbox, 
        ConfLink(new ConfLinkRclRep(config, "filtermaxseconds", &m_sk)), 
        tr("Max. filter exec. time (S)"),
        tr("External filters working longer than this will be "
           "aborted. This is for the rare case (ie: postscript) "
           "where a document could cause a filter to loop. "
           "Set to -1 for no limit.\n"), -1, 10000);
    m_widgets.push_back(efiltmaxsecs);
    gl1->addWidget(efiltmaxsecs, gridy++, 1);

    vboxLayout->addWidget(m_groupbox);
    subDirChanged(0, 0);
}

void ConfSubPanelW::reloadAll()
{
    for (list<ConfParamW*>::iterator it = m_widgets.begin();
	 it != m_widgets.end(); it++) {
	(*it)->loadValue();
    }
}

void ConfSubPanelW::subDirChanged(QListWidgetItem *current, QListWidgetItem *)
{
    LOGDEB("ConfSubPanelW::subDirChanged\n" );
	
    if (current == 0 || current->text() == "") {
	m_sk = "";
	m_groupbox->setTitle(tr("Global"));
    } else {
	m_sk = (const char *) current->text().toUtf8();
	m_groupbox->setTitle(current->text());
    }
    LOGDEB("ConfSubPanelW::subDirChanged: now ["  << (m_sk) << "]\n" );
    reloadAll();
}

void ConfSubPanelW::subDirDeleted(QString sbd)
{
    LOGDEB("ConfSubPanelW::subDirDeleted("  << ((const char *)sbd.toUtf8()) << ")\n" );
    if (sbd == "") {
	// Can't do this, have to reinsert it
	QTimer::singleShot(0, this, SLOT(restoreEmpty()));
	return;
    }
    // Have to delete all entries for submap
    m_config->eraseKey((const char *)sbd.toUtf8());
}

void ConfSubPanelW::restoreEmpty()
{
    LOGDEB("ConfSubPanelW::restoreEmpty()\n" );
    m_subdirs->getListBox()->insertItem(0, "");
}

} // Namespace confgui

