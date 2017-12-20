/*
   Copyright (C) 2017 Arto Hyvättinen

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

#include <cmath>

#include "poistaja.h"
#include "ui_poistaja.h"

#include "kirjaus/ehdotusmodel.h"
#include "raportti/raportinkirjoittaja.h"

#include <QSqlQuery>
#include <QDebug>

Poistaja::Poistaja(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Poistaja)
{
    ui->setupUi(this);
}

Poistaja::~Poistaja()
{
    delete ui;
}

bool Poistaja::teeSumuPoistot(Tilikausi kausi)
{
    Poistaja poistoDlg;
    return poistoDlg.sumupoistaja(kausi);
}

bool Poistaja::sumupoistaja(Tilikausi kausi)
{
    EhdotusModel ehdotus;
    RaportinKirjoittaja kirjoittaja;

    kirjoittaja.asetaOtsikko("POISTOLASKELMA");
    kirjoittaja.asetaKausiteksti( kausi.paattyy().toString(Qt::SystemLocaleShortDate));

    kirjoittaja.lisaaSarake("TUN123456/31.12.9999");
    kirjoittaja.lisaaVenyvaSarake();
    kirjoittaja.lisaaEurosarake();
    kirjoittaja.lisaaSarake("Poistosääntö");
    kirjoittaja.lisaaEurosarake();
    kirjoittaja.lisaaEurosarake();

    RaporttiRivi otsikko;
    otsikko.lisaa("Tili/Pvm");
    otsikko.lisaa("Nimike");
    otsikko.lisaa("Saldo ennen",1, true);
    otsikko.lisaa("Poistosääntö");
    otsikko.lisaa("Poisto",1,true);
    otsikko.lisaa("Saldo jälkeen",1,true);
    kirjoittaja.lisaaOtsake(otsikko);


    for( int i=0; i < kp()->tilit()->rowCount(QModelIndex()); i++)
    {
        Tili tili = kp()->tilit()->tiliIndeksilla(i);
        if( tili.onko(TiliLaji::MENOJAANNOSPOISTO))
        {
            int saldo = tili.saldoPaivalle( kausi.paattyy());

            if( !saldo)
                continue;

            int poistoprosentti = tili.json()->luku("Menojaannospoisto");
            int poisto = std::round( saldo * poistoprosentti / 100.0 );
            int jalkeen = saldo - poisto;

            RaporttiRivi rr;
            rr.lisaaLinkilla(RaporttiRiviSarake::TILI_LINKKI, tili.id(), QString::number(tili.numero()) );
            rr.lisaa(tili.nimi());
            rr.lisaa(saldo);
            rr.lisaa( tr("%1 %").arg( poistoprosentti ));
            rr.lisaa( poisto );
            rr.lisaa( jalkeen );
            rr.lihavoi();

            kirjoittaja.lisaaRivi(rr);
            kirjoittaja.lisaaRivi();

        }
        else if( tili.onko( TiliLaji::TASAERAPOISTO))
        {
            // Nyt sitten haetaan tilin tase-erät ;)

            int saldo = tili.saldoPaivalle( kausi.paattyy());

            if( !saldo)
                continue;

            RaporttiRivi tiliRivi;
            tiliRivi.lisaaLinkilla(RaporttiRiviSarake::TILI_LINKKI, tili.id(), QString::number(tili.numero()) );
            tiliRivi.lisaa( tili.nimi());
            tiliRivi.lihavoi();
            kirjoittaja.lisaaRivi(tiliRivi);

            EranValintaModel emodel;
            emodel.lataa(tili, false, kausi.paattyy());
            for(int eI=1; eI < emodel.rowCount(QModelIndex()); eI++)
            {
                QModelIndex eInd = emodel.index(eI, 0);

                int eranId = eInd.data(EranValintaModel::EraIdRooli).toInt();
                int eraSaldo = eInd.data(EranValintaModel::SaldoRooli).toInt();
                QDate eranPvm = eInd.data(EranValintaModel::PvmRooli).toDate();

                int alkuSnt = 0;
                int poistoKk = 0;

                QSqlQuery alkuKysely;
                alkuKysely.exec(QString("SELECT debetsnt, kreditsnt, json FROM vienti WHERE id=%1").arg(eranId) );
                if( alkuKysely.next())
                {
                    alkuSnt = alkuKysely.value("debetsnt").toInt() - alkuKysely.value("kreditsnt").toInt();
                    JsonKentta json( alkuKysely.value("json").toByteArray());
                    poistoKk = json.luku("Tasaerapoisto");
                }

                // Montako kuukautta on kulunut hankinnasta
                int kuukauttaKulunut = kausi.paattyy().year() * 12 + kausi.paattyy().month() -
                                       eranPvm.year() * 12 - eranPvm.month() + 1;



                // Laskennallinen poisto: Paljonko tähän asti voitaisiin poistaa
                int laskennallinenPoisto = alkuSnt * kuukauttaKulunut / poistoKk ;
                if( laskennallinenPoisto > alkuSnt)
                    laskennallinenPoisto = alkuSnt; // Poistetaan vain se, mitä on jäljellä ...


                int eraPoisto = laskennallinenPoisto - alkuSnt + eraSaldo;

                qDebug() << kuukauttaKulunut << " kk " << laskennallinenPoisto << " lapo " << poistoKk << " kk poistoaikaa " << alkuSnt << " alkuSnt" ;

                RaporttiRivi rr;
                rr.lisaa( eranPvm );
                rr.lisaa( eInd.data(EranValintaModel::SeliteRooli).toString());
                rr.lisaa( eraSaldo );
                if( poistoKk % 12)      // Poistoaika
                    rr.lisaa( tr( "%1 v %2 kk").arg(poistoKk / 12).arg(poistoKk % 12));
                else
                    rr.lisaa( tr("%1 v").arg(poistoKk / 12));
                rr.lisaa( eraPoisto);
                rr.lisaa( eraSaldo - eraPoisto );

                kirjoittaja.lisaaRivi(rr);
            }
            kirjoittaja.lisaaRivi();

        }
    }

    // Näytetään ehdotus



    ui->browser->setHtml( kirjoittaja.html());
    if( exec() )
    {
        // Kirjataan ...
        return true;
    }
    return false;
}

bool Poistaja::onkoPoistoja(Tilikausi kausi)
{
    // TODO : Onko poistot jo tehty???

    QSqlQuery kysely;
    kysely.exec( QString("SELECT sum(debetsnt) as db, sum(kreditsnt) as kr from viennit,tili where viennit.tili = tili.id and"
                         "(tili.tyyppi=\"APM\" or tili.tyyppi=\"APT\") and pvm <= \"%1\" ").arg(kausi.paattyy().toString(Qt::ISODate)) );
    if( kysely.next())
        return kysely.value("db").toInt() != kysely.value("kr").toInt();

    return true;
}