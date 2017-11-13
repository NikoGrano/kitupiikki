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

#ifndef ERANVALINTAMODEL_H
#define ERANVALINTAMODEL_H

#include <QAbstractListModel>
#include <QList>
#include "tili.h"

/**
 * @brief Yhden tase-erän tiedot EranValintaModel:issa
 */
struct TaseEraValittavaksi
{
    int eraId;
    QDate pvm;
    QString selite;
    int saldoSnt;
};

/**
 * @brief Tase-erän valintamodel
 *
 * Tase-erät kuvaavat tase-erittelyn kohteita. Jos seurattavalle tilille tehdään vienti,
 * joka ei kuulu aikaisempaan tase-erään, muodostuu uusi tase-erä. Tilin kirjaus voidaan
 * kohdistaa tälle erälle, ellei tase-erä ole jo tasan (debet=kredit)
 *
 */
class EranValintaModel : public QAbstractListModel
{
    Q_OBJECT

public:

    enum
    {
        EraIdRooli = Qt::UserRole + 1,
        PvmRooli = Qt::UserRole +2,
        SeliteRooli = Qt::UserRole +3,
        SaldoRooli = Qt::UserRole + 4,
    };


    EranValintaModel();

    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;


    void lataa(Tili tili);

private:
    QList<TaseEraValittavaksi> erat_;
};

#endif // ERANVALINTAMODEL_H