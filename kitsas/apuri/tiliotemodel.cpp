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
#include "tiliotemodel.h"
#include "db/kirjanpito.h"
#include "db/tilinvalintadialogi.h"
#include "db/tositetyyppimodel.h"
#include "db/tilityyppimodel.h"
#include "model/tositevienti.h"

TilioteModel::TilioteModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

QVariant TilioteModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // FIXME: Implement me!
    if( role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        switch (section)
        {
        case PVM:
            return tr("Pvm");
        case EURO:
            return tr("Euro");
        case TILI:
            return tr("Tili");
        case KOHDENNUS:
            return tr("Kohdennus");
        case SAAJAMAKSAJA:
            return tr("Saaja/Maksaja");
        case SELITE:
            return tr("Selite");

        }
    }
    return QVariant();
}

bool TilioteModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
{
    if (value != headerData(section, orientation, role)) {
        // FIXME: Implement me!
        emit headerDataChanged(orientation, section, section);
        return true;
    }
    return false;
}


int TilioteModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return rivit_.count();
}

int TilioteModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return 6;
}

QVariant TilioteModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    Tilioterivi rivi = rivit_.at(index.row());


    switch ( role ) {
    case Qt::DisplayRole :
        switch (index.column()) {
        case PVM:
            return rivi.pvm;
        case EURO:
            if( qAbs(rivi.euro) > 1e-5)
                return QString("%L1 €").arg( rivi.euro ,0,'f',2);
            return QString();
        case TILI:
            return kp()->tilit()->tiliNumerolla( rivi.tili ).nimiNumero();
        case KOHDENNUS:
        {
            if( rivi.laskupvm.isValid())
                return rivi.laskupvm;            

            QString txt;
            if( rivi.kohdennus )
                txt = kp()->kohdennukset()->kohdennus(rivi.kohdennus).nimi() + " ";
            QStringList merkkausList;
            for(auto mid : rivi.merkkaukset)
                merkkausList.append( kp()->kohdennukset()->kohdennus(mid.toInt()).nimi() );
            txt.append( merkkausList.join(", ") );
            return txt;
        }
        case SAAJAMAKSAJA:
            return rivi.saajamaksaja;
        case SELITE:
            return  rivi.selite;
        default:
            return QVariant();

    }
    case Qt::EditRole :
        switch ( index.column())
        {
        case PVM:
            return rivi.pvm;
        case TILI:
            return rivi.tili;
        case EURO:
            return qRound64( rivi.euro * 100 );
        case KOHDENNUS:
            return rivi.kohdennus;
        case SAAJAMAKSAJA:
            return rivi.saajamaksajaId;
        case SELITE:
            return rivi.selite;
        }

    case Qt::TextAlignmentRole:
        if( index.column()==EURO)
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        else
            return QVariant( Qt::AlignLeft | Qt::AlignVCenter);

    case Qt::DecorationRole:
        if( index.column() == KOHDENNUS) {
            if( rivi.laskupvm.isValid())
                return QIcon(":/pic/lasku.png");
        }

    }
    return QVariant();
}

bool TilioteModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (data(index, role) != value) {

        if( role == Qt::EditRole) {
            switch (index.column()) {
                case PVM :
                    rivit_[index.row()].pvm = value.toDate();
                break;
            case TILI: {
                Tili uusitili;
                if( value.toInt())
                    uusitili = kp()->tilit()->tiliNumerolla( value.toInt());
                else if(!value.toString().isEmpty() && value.toString() != " ")
                    uusitili = TilinValintaDialogi::valitseTili(value.toString());
                else
                    uusitili = TilinValintaDialogi::valitseTili( QString());
                rivit_[ index.row()].tili = uusitili.numero();
                break;
                }
            case EURO:
                rivit_[ index.row()].euro = value.toDouble() ;
                break;
            case KOHDENNUS:
                rivit_[ index.row()].kohdennus = value.toInt();
                break;
            case SELITE:
                rivit_[index.row()].selite = value.toString();
            }
        }

        emit dataChanged(index, index, QVector<int>() << role);
        return true;
    }
    return false;
}

Qt::ItemFlags TilioteModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    if( rivit_.at(index.row()).harmaa)
        return Qt::NoItemFlags;

    if( index.column() == KOHDENNUS )
    {
        Tili tili = kp()->tilit()->tiliNumerolla( rivit_.at(index.row()).tili );
        if( !tili.onko(TiliLaji::TULOS))
            return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
    if( index.column() == SAAJAMAKSAJA)
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void TilioteModel::lisaaRivi(const TilioteModel::Tilioterivi &rivi)
{
    beginInsertRows( QModelIndex(), rowCount(), rowCount());
    rivit_.append(rivi);
    endInsertRows();
}

void TilioteModel::poistaRivi(int rivi)
{
    beginRemoveRows(QModelIndex(), rivi, rivi);
    rivit_.removeAt(rivi);
    endRemoveRows();
}

void TilioteModel::muokkaaRivi(int rivi, const TilioteModel::Tilioterivi &data)
{
    rivit_[rivi] = data;
    emit dataChanged( index(rivi,PVM), index(rivi,SELITE) );
}

QVariantList TilioteModel::viennit(int tilinumero) const
{
    QVariantList lista;

    for(auto rivi : rivit_) {
        if( qAbs( rivi.euro ) > 1e-5 && rivi.tili ) {
            TositeVienti pankki;
            TositeVienti tili;

            pankki.setPvm( rivi.pvm );
            tili.setPvm( rivi.pvm );

            pankki.setTili(tilinumero);
            tili.setTili( rivi.tili );

            if( rivi.euro > 0.0) {
                pankki.setDebet( rivi.euro);
                tili.setKredit( rivi.euro);
            } else {
                pankki.setKredit( 0.0 - rivi.euro);
                tili.setDebet( 0.0 - rivi.euro);
            }

            tili.setKohdennus( rivi.kohdennus);

            if( rivi.era.value("id").toInt())
                tili.setEra( rivi.era );

            if( rivi.era.contains("id") && rivi.era.value("id").toInt() > -1) {
                pankki.setTyyppi( TositeVienti::SUORITUS + TositeVienti::VASTAKIRJAUS );
                tili.setTyyppi( TositeVienti::SUORITUS + TositeVienti::KIRJAUS);
            } else if( kp()->tilit()->tili(tili.tili())->onko(TiliLaji::TASE)) {
                pankki.setTyyppi( TositeVienti::SIIRTO );
                tili.setTyyppi( TositeVienti::SIIRTO );
            } else if( rivi.euro > 0.0 ) {
                pankki.setTyyppi( TositeVienti::MYYNTI + TositeVienti::VASTAKIRJAUS );
                tili.setTyyppi( TositeVienti::MYYNTI + TositeVienti::KIRJAUS);
            } else {
                pankki.setTyyppi( TositeVienti::OSTO + TositeVienti::VASTAKIRJAUS);
                tili.setTyyppi( TositeVienti::OSTO + TositeVienti::KIRJAUS);
            }

            pankki.setSelite( rivi.selite );
            tili.setSelite( rivi.selite );

            tili.setJaksoalkaa( rivi.jaksoalkaa );
            tili.setJaksoloppuu( rivi.jaksoloppuu );

            tili.setMerkkaukset( rivi.merkkaukset);
            tili.setKohdennus( rivi.kohdennus );

            // TODO: Arkistotunnus, tilinumero, viite yms. metatieto
            pankki.setArkistotunnus( rivi.arkistotunnus );

            if( rivi.saajamaksajaId){
                pankki.set(TositeVienti::KUMPPANI, rivi.saajamaksajaId);
                tili.set(TositeVienti::KUMPPANI, rivi.saajamaksajaId);
            } else if( !rivi.saajamaksaja.isEmpty()) {
                pankki.setKumppani( rivi.saajamaksaja);
                tili.setKumppani( rivi.saajamaksaja );
            }

            int indeksi = lista.count();

            // Erikoisrivit
            // Vastaava käsittely pitäisi lisätä myös Siirto-tositteille
            double osuusErasta = 0.0;
            for(auto item : rivi.alkuperaisetViennit) {
                TositeVienti evienti(item.toMap());
                if( evienti.tyyppi() % 100 == TositeVienti::VASTAKIRJAUS && qAbs(rivi.euro) > 1e-5) {
                    osuusErasta = qAbs( rivi.euro / (evienti.debet() - evienti.kredit()) );
                } else if( evienti.alvKoodi() / 100 == 4) {
                    // Maksuperusteinen kohdentamaton
                    qlonglong sentit = qRound64( osuusErasta * (evienti.kredit() - evienti.debet()) * 100.0);
                    TositeVienti mpDebet;
                    mpDebet.setPvm(rivi.pvm);
                    mpDebet.setTili(evienti.tili());
                    if( sentit > 0)
                        mpDebet.setDebet(sentit);
                    else
                        mpDebet.setKredit(0-sentit);
                    mpDebet.setSelite(rivi.selite);
                    mpDebet.setEra(evienti.era());
                    mpDebet.setAlvKoodi(AlvKoodi::TILITYS);
                    lista.append(mpDebet);

                    TositeVienti mpKredit;
                    mpKredit.setPvm(rivi.pvm);
                    if( evienti.tili() == kp()->tilit()->tiliTyypilla(TiliLaji::KOHDENTAMATONALVVELKA).numero() ||
                        evienti.alvKoodi() == AlvKoodi::ENNAKKOLASKU_MYYNTI + AlvKoodi::MAKSUPERUSTEINEN_KOHDENTAMATON) {
                        mpKredit.setTili(kp()->tilit()->tiliTyypilla(TiliLaji::ALVVELKA).numero());
                        mpKredit.setAlvKoodi(evienti.alvKoodi() % 100 + AlvKoodi::ALVKIRJAUS);
                    } else if( evienti.tili() == kp()->tilit()->tiliTyypilla(TiliLaji::KOHDENTAMATONALVSAATAVA).numero()) {
                        mpKredit.setTili(kp()->tilit()->tiliTyypilla(TiliLaji::ALVSAATAVA).numero());
                        mpKredit.setAlvKoodi(evienti.alvKoodi() % 100 + AlvKoodi::ALVVAHENNYS);
                    }
                    mpKredit.setAlvProsentti(evienti.alvProsentti());
                    if( sentit > 0)
                        mpKredit.setKredit(sentit);
                    else
                        mpKredit.setDebet(0-sentit);
                    mpKredit.setSelite(rivi.selite);
                    lista.append(mpKredit);

                    tili.set(TositeVienti::ALKUPVIENNIT, rivi.alkuperaisetViennit);
                }
            }

            lista.insert(indeksi,pankki);
            lista.insert(indeksi+1,tili);
        }
    }
    return lista;
}

void TilioteModel::lataa(const QVariantList &lista)
{
    beginResetModel();
    rivit_.clear();
    // Haetaan ainoastaan joka toinen rivi eli vientirivit, kaikki muuthan koskeekin pankkitiliä
    for(int i=1; i < lista.count(); i++)
    {
        TositeVienti vienti = lista.at(i).toMap();

        if( vienti.tyyppi() % 100 != 1)
            continue;

        TositeVienti pankki = lista.at(i-1).toMap();
        Tilioterivi rivi;

        bool meno = pankki.kredit() > 0;

        rivi.pvm = vienti.pvm();
        rivi.euro = meno ? 0 - pankki.kredit() : pankki.debet();
        rivi.tili = vienti.tili();
        rivi.kohdennus = vienti.kohdennus();
        rivi.merkkaukset = vienti.merkkaukset();
        rivi.era = vienti.era();
        rivi.saajamaksaja = vienti.value("kumppani").toMap().value("nimi").toString();
        rivi.saajamaksajaId = vienti.value("kumppani").toMap().value("id").toInt();
        rivi.jaksoalkaa = vienti.jaksoalkaa();
        rivi.jaksoloppuu = vienti.jaksoloppuu();

        if( vienti.eraId() ) {
            rivi.laskupvm = vienti.value("era").toMap().value("pvm").toDate();
        }

        rivi.selite = vienti.selite();
        rivi.arkistotunnus = pankki.arkistotunnus();
        rivi.alkuperaisetViennit = vienti.data(TositeVienti::ALKUPVIENNIT).toList();

        rivit_.append(rivi);
    }
    endResetModel();
}

void TilioteModel::tuo(const QVariantList tuotavat)
{
    tuotavat_ = tuotavat;
}

void TilioteModel::lataaHarmaat(int tili, const QDate &mista, const QDate &mihin)
{
    KpKysely *kysely = kpk("/viennit");
    kysely->lisaaAttribuutti("tili", tili);
    kysely->lisaaAttribuutti("alkupvm", mista);
    kysely->lisaaAttribuutti("loppupvm", mihin);

    connect( kysely, &KpKysely::vastaus, this, &TilioteModel::harmaatSaapuu);
    kysely->kysy();
}

void TilioteModel::harmaatSaapuu(QVariant *data)
{
    beginResetModel();

    // Poistetaan kaikki vanhat harmaat
    for(int i=rivit_.count()-1; i>-1; i--) {
        if( rivit_.at(i).harmaa )
            rivit_.removeAt(i);
    }

    // Sijoitetaan uudet harmaat loppuun
    QVariantList lista = data->toList();

    for(auto listalla : lista) {
        QVariantMap map = listalla.toMap();

        if( map.value("tosite").toMap().value("tyyppi").toInt() == TositeTyyppi::TILIOTE )
            continue;       // On jo tiliotteelta

        Tilioterivi rivi;
        rivi.pvm = map.value("pvm").toDate();
        if( map.value("debet").toDouble() > 1e-5)
            rivi.euro = map.value("debet").toDouble();
        else
            rivi.euro = 0 - map.value("kredit").toDouble();

        rivi.selite = map.value("selite").toString();
        rivi.saajamaksaja = map.value("kumppani").toMap().value("nimi").toString();
        rivi.saajamaksajaId = map.value("kumppani").toMap().value("id").toInt();
        rivi.arkistotunnus = map.value("arkistotunnus").toString();
        rivi.harmaa = true;

        rivit_.append(rivi);
    }

    if( !tuotavat_.isEmpty())
        teeTuonti();

    endResetModel();
}

void TilioteModel::teeTuonti()
{
    // Lisätään ensin kaikki listalle
    // ja sitten käydään vielä läpi harmaat ja tehdään niiden kautta poistamiset

    int ennentuontia = rivit_.count();

    for(auto item : tuotavat_) {
        QVariantMap map = item.toMap();

        Tilioterivi rivi;
        rivi.pvm = map.value("pvm").toDate();
        rivi.euro = map.value("euro").toDouble();
        rivi.selite = map.value("selite").toString();

        rivi.saajamaksaja = map.value("saajamaksaja").toString();
        rivi.saajamaksajaId = map.value("saajamaksajaid").toInt();

        rivi.arkistotunnus = map.value("arkistotunnus").toString();

        rivi.era = map.value("era").toMap();
        rivi.laskupvm = map.value("era").toMap().value("pvm").toDate();
        rivi.tili = map.value("tili").toInt();
        rivi.tilinumero = map.value("iban").toString();

        rivit_.append(rivi);
    }

    // Siivotaan ne, jotka oli kirjattu käsin tililtä
    for(int i=0; i < ennentuontia; i++)
        if( rivit_.value(i).harmaa)
            siivoa(i, ennentuontia);
}

void TilioteModel::siivoa(int harmaarivi, int myohemmat)
{
    // Yritetään ensin arkistotunnuksella
    // Näin pitäisi löytyä tuplasti tiliotteelta tuodut
    QString arkistotunnus = rivi(harmaarivi).arkistotunnus;

    if( !arkistotunnus.isEmpty()) {
        for(int i=myohemmat; i < rivit_.count(); i++) {
            if( arkistotunnus == rivi(i).arkistotunnus ) {
                rivit_.removeAt(i);
                return;
            }
        }
    }

    // Jos ei löydetty arkistotunnuksella, niin sitten yritetään päiväyksen ja määrän
    // yhdistelmällä

    QList<int> sopivat;

    QDate pvm = rivi(harmaarivi).pvm;
    double euro = rivi(harmaarivi).euro;

    for( int i=myohemmat; i < rivit_.count(); i++) {
        QDate vrtPvm = rivi(i).pvm;
        double vrtEuro = rivi(i).euro;
        if( pvm == vrtPvm && qAbs( euro - vrtEuro) < 1e-3 &&
            arkistotunnus.isEmpty() )
            sopivat.append(i);
    }

    if( sopivat.count() == 1) {
        rivit_.removeAt( sopivat.first() );
        return;
    }

    QList<int> ksopivat;

    // Jos on useampia muuten sopivia, yritetään saada oikea kumppani
    for( int sopiva : sopivat) {
        if( rivi(sopiva).saajamaksajaId &&
            rivi(sopiva).saajamaksajaId == rivi(harmaarivi).saajamaksajaId ) {
            ksopivat.append(sopiva);
        }
    }

    if( ksopivat.count() == 1) {
        rivit_.removeAt( ksopivat.first() );
    }
}
