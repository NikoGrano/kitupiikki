/*
   Copyright (C) 2019 Arto Hyvättinen

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include "kumppanituotewidget.h"
#include "ui_kumppanituotewidget.h"

#include "asiakkaatmodel.h"
#include "tuotemodel.h"
#include <QSortFilterProxyModel>

#include "rekisteri/asiakastoimittajadlg.h"
#include "rekisteri/ryhmatmodel.h"
#include "vakioviite/vakioviitemodel.h"
#include "vakioviite/vakioviitedlg.h"
#include "tuotedialogi.h"

#include "db/kirjanpito.h"
#include "db/yhteysmodel.h"

#include "rekisteri/rekisterituontidlg.h"
#include "rekisteri/rekisterinvienti.h"
#include "raportti/raportinkirjoittaja.h"
#include "naytin/naytinikkuna.h"

#include <QDebug>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>

KumppaniTuoteWidget::KumppaniTuoteWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::KumppaniTuoteWidget),
    proxy_(new QSortFilterProxyModel(this)),
    asiakkaat_( new AsiakkaatModel(this)),
    vakioviitteet_( new VakioViiteModel(this))
{
    ui->setupUi(this);

    proxy_->setDynamicSortFilter(true);
    proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy_->setSortRole(Qt::EditRole);
    ui->view->setSortingEnabled(true);
    ui->view->setModel(proxy_);

    connect( ui->view->selectionModel(), &QItemSelectionModel::selectionChanged,
             this, &KumppaniTuoteWidget::ilmoitaValinta);    

    connect( ui->uusiNappi, &QPushButton::clicked, this, &KumppaniTuoteWidget::uusi);
    connect( ui->muokkaaNappi, &QPushButton::clicked, this, &KumppaniTuoteWidget::muokkaa);
    connect( ui->poistaNappi, &QPushButton::clicked, this, &KumppaniTuoteWidget::poista);
    connect( ui->view, &QTableView::doubleClicked, this, &KumppaniTuoteWidget::muokkaa);    

    connect( ui->tuoNappi, &QPushButton::clicked, this, &KumppaniTuoteWidget::tuo);
    connect( ui->VieNappi, &QPushButton::clicked, this, &KumppaniTuoteWidget::vie);
    connect( ui->raporttiButton, &QPushButton::clicked, this, &KumppaniTuoteWidget::raportti);

    ui->muokkaaNappi->setEnabled(false);
    ui->poistaNappi->setEnabled(false);



}

KumppaniTuoteWidget::~KumppaniTuoteWidget()
{
    delete ui;
}

void KumppaniTuoteWidget::nayta(int valilehti)
{
    valilehti_ = valilehti;

      ui->tuoNappi->setVisible( valilehti == ASIAKKAAT || valilehti == TOIMITTAJAT || valilehti == REKISTERI);
      ui->VieNappi->setVisible( valilehti == ASIAKKAAT || valilehti == TOIMITTAJAT || valilehti == REKISTERI);


    bool muokkausoikeus = false;
    if( valilehti == RYHMAT )
        muokkausoikeus = kp()->yhteysModel()->onkoOikeutta(YhteysModel::RYHMAT);
    else if( valilehti == VAKIOVIITTEET)
        muokkausoikeus = kp()->yhteysModel()->onkoOikeutta(YhteysModel::ASETUKSET);
    else if(valilehti == TUOTTEET)
        muokkausoikeus = kp()->yhteysModel()->onkoOikeutta(YhteysModel::TUOTTEET);
    else
        muokkausoikeus = kp()->yhteysModel()->onkoOikeutta(YhteysModel::TOSITE_LUONNOS | YhteysModel::TOSITE_MUOKKAUS |
                                                           YhteysModel::LASKU_LAATIMINEN | YhteysModel::LASKU_LAHETTAMINEN);
    ui->uusiNappi->setVisible(muokkausoikeus);
    ui->poistaNappi->setVisible(muokkausoikeus);
    ui->muokkaaNappi->setVisible(muokkausoikeus);

    paivita();
}

void KumppaniTuoteWidget::suodata(const QString &suodatus)
{
    proxy_->setFilterFixedString( suodatus );
}

void KumppaniTuoteWidget::suodataRyhma(int ryhma)
{
    ryhma_ = ryhma;
    paivita();
}

void KumppaniTuoteWidget::ilmoitaValinta()
{    
    bool napitkaytossa = true;
    if( ui->view->selectionModel()->selectedRows(0).count() ) {        
        if( valilehti_ == RYHMAT) {
            emit ryhmaValittu( ui->view->selectionModel()->selectedRows(0).value(0).data(RyhmatModel::IdRooli).toInt());
            napitkaytossa = ui->view->selectionModel()->selectedRows(0).value(0).row() > 0;
        } else {
            emit kumppaniValittu(  ui->view->selectionModel()->selectedRows(0).value(0).data().toString() );
        }
    } else {
        if( valilehti_ == RYHMAT)
            emit ryhmaValittu(0);
        else
            emit kumppaniValittu("");
        napitkaytossa = false;
    }
    ui->muokkaaNappi->setEnabled( napitkaytossa );
    ui->poistaNappi->setEnabled( napitkaytossa );
}

void KumppaniTuoteWidget::uusi()
{
    if( valilehti_ == TUOTTEET ) {
        TuoteDialogi *dlg = new TuoteDialogi(this);
        dlg->uusi();
    } else if( valilehti_ == RYHMAT) {
        QString nimi = QInputDialog::getText(this, tr("Uusi ryhmä"), tr("Ryhmän nimi"));
        if( !nimi.isEmpty()) {
            QVariantMap uusiryhma;
            uusiryhma.insert("nimi", nimi);
            KpKysely* kysely = kpk("/ryhmat", KpKysely::POST);
            connect(kysely, &KpKysely::vastaus, kp()->ryhmat(), &RyhmatModel::paivita);
            kysely->kysy(uusiryhma);
        }
    } else if( valilehti_ == VAKIOVIITTEET ) {
        VakioViiteDlg dlg(this);
        dlg.uusi();
        vakioviitteet_->lataa();
    } else {
        AsiakasToimittajaDlg *dlg = new AsiakasToimittajaDlg(this);
        connect( dlg, &AsiakasToimittajaDlg::tallennettu, this, &KumppaniTuoteWidget::paivita);
        dlg->uusi();
        if( ryhma_)
            dlg->lisaaRyhmaan(ryhma_);
    }
}

void KumppaniTuoteWidget::muokkaa()
{
    if( valilehti_ == TUOTTEET) {
        TuoteDialogi *dlg = new TuoteDialogi(this);
        dlg->muokkaa( ui->view->currentIndex().data(TuoteModel::MapRooli).toMap()  );        
    } else if (valilehti_ == RYHMAT) {
        int ryhmaid = ui->view->currentIndex().data(RyhmatModel::IdRooli).toInt();
        if( !ryhmaid) return;
        QString nimi = QInputDialog::getText(this, tr("Muokkaa ryhmää"), tr("Ryhmän nimi"),QLineEdit::Normal,
                                             ui->view->currentIndex().data(Qt::DisplayRole).toString());
        if( !nimi.isEmpty()) {        
            QVariantMap muokattu;
            muokattu.insert("nimi", nimi);
            KpKysely* kysely = kpk(QString("/ryhmat/%1").arg(ryhmaid), KpKysely::PUT);
            connect(kysely, &KpKysely::vastaus, kp()->ryhmat(), &RyhmatModel::paivita);
            kysely->kysy(muokattu);
        }
    } else if( valilehti_ == VAKIOVIITTEET) {
        VakioViiteDlg dlg(this);
        dlg.muokkaa( ui->view->currentIndex().data(VakioViiteModel::MapRooli).toMap() );
        vakioviitteet_->lataa();
    } else {
        AsiakasToimittajaDlg *dlg = new AsiakasToimittajaDlg(this);
        dlg->muokkaa( ui->view->currentIndex().data(AsiakkaatModel::IdRooli).toInt() );
        connect( dlg, &AsiakasToimittajaDlg::tallennettu, this, &KumppaniTuoteWidget::paivita);
    }
}

void KumppaniTuoteWidget::poista()
{
    if( valilehti_ == TUOTTEET) {
        int tuoteid = ui->view->currentIndex().data(TuoteModel::IdRooli).toInt();
        if( tuoteid &&
            QMessageBox::question(this, tr("Tuotteen poistaminen"),tr("Haluatko varmasti poistaa tuotteen?")) == QMessageBox::Yes) {
            kp()->tuotteet()->poistaTuote(tuoteid);
        }
    } else if( valilehti_ == RYHMAT) {
        if( QMessageBox::question(this, tr("Ryhmän poistaminen"),tr("Haluatko varmasti poistaa ryhmän?")) == QMessageBox::Yes) {
            int ryhmaid = ui->view->currentIndex().data(RyhmatModel::IdRooli).toInt();
            KpKysely *kysely = kpk(QString("/ryhmat/%1").arg(ryhmaid), KpKysely::DELETE );
            connect( kysely, &KpKysely::vastaus, kp()->ryhmat(), &RyhmatModel::paivita);
            kysely->kysy();
        }
    } else if( valilehti_ == VAKIOVIITTEET) {
        if( QMessageBox::question(this, tr("Vakioviitteen poistaminen"),tr("Haluatko varmasti poistaa vakioviitteen?")) == QMessageBox::Yes) {
            int viite = ui->view->currentIndex().data(VakioViiteModel::ViiteRooli).toInt();
            KpKysely *kysely = kpk(QString("/vakioviitteet/%1").arg(viite), KpKysely::DELETE );
            connect( kysely, &KpKysely::vastaus, vakioviitteet_, &VakioViiteModel::lataa);
            kysely->kysy();
        }
    } else {
        int kid = ui->view->currentIndex().data(AsiakkaatModel::IdRooli).toInt();
        if( kid ) {
            KpKysely *kysely = kpk(QString("/kumppanit/%1").arg(kid), KpKysely::DELETE );
            connect( kysely, &KpKysely::vastaus, this, &KumppaniTuoteWidget::paivita);
            kysely->kysy();
        }
    }
}

void KumppaniTuoteWidget::paivita()
{
    if( valilehti_ == TUOTTEET)
        proxy_->setSourceModel( kp()->tuotteet() );
    else if( valilehti_ == VAKIOVIITTEET)
        proxy_->setSourceModel( vakioviitteet_ );
    else if (valilehti_ == RYHMAT) {
        proxy_->setSourceModel( kp()->ryhmat() );
        ui->view->selectRow(0);
    } else
        proxy_->setSourceModel( asiakkaat_);

    if( valilehti_ == REKISTERI) {
        if( ryhma_)
            asiakkaat_->suodataRyhma(ryhma_);
        else
            asiakkaat_->paivita(AsiakkaatModel::REKISTERI);
    } else if( valilehti_ == ASIAKKAAT )
        asiakkaat_->paivita(AsiakkaatModel::ASIAKKAAT);
    else if( valilehti_ == TOIMITTAJAT)
        asiakkaat_->paivita(AsiakkaatModel::TOIMITTAJAT);
    else if( valilehti_ == VAKIOVIITTEET)
        vakioviitteet_->lataa();

    ui->view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ilmoitaValinta();
}

void KumppaniTuoteWidget::tuo()
{
    if( valilehti_ == ASIAKKAAT || valilehti_ == TOIMITTAJAT || valilehti_ == REKISTERI) {
        QString tiedosto = QFileDialog::getOpenFileName(this, tr("Tuonti csv-tiedostosta"),QString(),
                                                        tr("Csv-tiedostot (*.csv);;Kaikki tiedostot (*)"));
        if( !tiedosto.isEmpty()) {
            RekisteriTuontiDlg dlg(tiedosto,this);
            dlg.exec();
            paivita();
        }
    }
}

void KumppaniTuoteWidget::vie()
{
    if( valilehti_ == ASIAKKAAT || valilehti_ == TOIMITTAJAT || valilehti_ == REKISTERI) {
        QString tiedosto = QFileDialog::getSaveFileName(this, tr("Vienti csv-tiedostoon"), QString(),
                                                        tr("Csv-tiedostot (*.csv);;Kaikki tiedostot (*)"));
        if( !tiedosto.isEmpty()) {
            RekisterinVienti::vieRekisteri(ui->view->model(), tiedosto);
        }
    }
}

void KumppaniTuoteWidget::raportti()
{
    QAbstractItemModel *model = proxy_;
    RaportinKirjoittaja rk;
    switch (valilehti_) {
    case ASIAKKAAT: rk.asetaOtsikko(tr("Asiakkaat")); break;
    case TOIMITTAJAT: rk.asetaOtsikko(tr("Toimittajat")); break;
    case REKISTERI: rk.asetaOtsikko(tr("Rekisteri")); break;
    case RYHMAT: rk.asetaOtsikko(tr("Ryhmät")); break;
    case VAKIOVIITTEET: rk.asetaOtsikko(tr("Vakioviitteet")); break;
    }

    rk.lisaaVenyvaSarake();
    if( valilehti_ == VAKIOVIITTEET) {
        rk.lisaaSarake("XXXXXXXX");
        rk.lisaaVenyvaSarake(50);
        rk.lisaaSarake("Kohdennusnimipitkä");
    } else {
        for(int i=1; i<model->columnCount();i++)
            rk.lisaaEurosarake();
    }

    RaporttiRivi otsikko;
    for(int i=0; i<model->columnCount(); i++)
        otsikko.lisaa( model->headerData(i, Qt::Horizontal).toString(), 1, i && valilehti_ != VAKIOVIITTEET);
    rk.lisaaOtsake(otsikko);

    for(int i=0; i<model->rowCount();i++) {
        if(valilehti_ == RYHMAT && !i) continue; // "Kaikki ryhmät"

        RaporttiRivi rivi;
        for(int j=0; j<model->columnCount(); j++) {
           rivi.lisaa(model->index(i,j).data().toString(),1,j && valilehti_ != VAKIOVIITTEET);
        }
        rk.lisaaRivi(rivi);
    }

    NaytinIkkuna::naytaRaportti(rk);

}




