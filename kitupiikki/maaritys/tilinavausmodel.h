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

#ifndef TILINAVAUSMODEL_H
#define TILINAVAUSMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QMap>

#include "db/kirjanpito.h"

/**
 * @brief Tilinavaukset
 *
 * Tätä modelia käytetään tilinavausten syöttämiseen määrittelynäkymässä
 *
 */
class TilinavausModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Sarake
    {
        NRO, NIMI, SALDO
    };

    enum
    {
        KaytossaRooli = Qt::UserRole + 1
    };

    TilinavausModel();

    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    bool onkoMuokattu() const { return muokattu_; }

public slots:
    void lataa();
    bool tallenna();

    void paivitaInfo();

signals:
    void infoteksti(QString teksti);

protected:
    QMap<int,qlonglong> saldot;
    bool muokattu_;
    int kaudenTulosIndeksi_ = 0;
};

#endif // TILINAVAUSMODEL_H
