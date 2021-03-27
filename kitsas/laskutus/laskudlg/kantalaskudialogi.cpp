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
#include "kantalaskudialogi.h"
#include "ui_laskudialogi.h"

#include "model/tosite.h"

#include "db/kirjanpito.h"

#include "db/tositetyyppimodel.h"

#include "../ryhmalasku/kielidelegaatti.h"
#include "model/tositerivit.h"
#include "model/tositeloki.h"
#include "model/tositeviennit.h"

#include "naytin/naytinikkuna.h"
#include "../viitenumero.h"

#include "../vakioviite/vakioviitemodel.h"
#include "../huoneisto/huoneistomodel.h"
#include "../tulostus/laskuntulostaja.h"

#include <QJsonDocument>
#include <QSettings>

#include <QPdfWriter>
#include <QPainter>
#include <QMessageBox>

#include "rekisteri/maamodel.h"


KantaLaskuDialogi::KantaLaskuDialogi(Tosite *tosite, QWidget *parent) :
    QDialog(parent),
    ui( new Ui::LaskuDialogi),
    tosite_(tosite),
    huoneistot_( kp()->huoneistot() )
{
    ui->setupUi(this);
    tosite_->setParent(this);

    setAttribute(Qt::WA_DeleteOnClose);
    resize(800,600);
    restoreGeometry( kp()->settings()->value("LaskuDialogi").toByteArray());

    alustaUi();
    teeConnectit();

    tositteelta();

}

KantaLaskuDialogi::~KantaLaskuDialogi() {
    kp()->settings()->setValue("LaskuDialogi", saveGeometry());
    delete ui;
}

QString KantaLaskuDialogi::asiakkaanAlvTunnus() const
{
    return ladattuAsiakas_.value("alvtunnus").toString();
}

int KantaLaskuDialogi::maksutapa() const
{
    return ui->maksuCombo->currentData().toInt();
}

void KantaLaskuDialogi::tulosta(QPagedPaintDevice *printer) const
{
    printer->setPageSize( QPdfWriter::A4);
    printer->setPageMargins( QMarginsF(10,10,10,10), QPageLayout::Millimeter );

    QPainter painter( printer);

    LaskunTulostaja tulostaja( kp());
    Tosite tulostettava;
    tulostettava.lataa(tosite_->tallennettava());

    tulostaja.tulosta(tulostettava, printer, &painter);
    painter.end();
}


void KantaLaskuDialogi::alustaUi()
{
    KieliDelegaatti::alustaKieliCombo(ui->kieliCombo);
    ui->jaksoDate->setNull();
    ui->tilausPvm->setNull();

    ui->maksuaikaSpin->setValue(kp()->asetukset()->luku(AsetusModel::LaskuMaksuaika,14) );
    ui->saateEdit->setPlainText( kp()->asetukset()->asetus(AsetusModel::EmailSaate) );
    ui->viivkorkoSpin->setValue( kp()->asetukset()->asetus(AsetusModel::LaskuPeruskorko).toDouble() + 7.0 );

    int lokiIndex = ui->tabWidget->indexOf( ui->tabWidget->findChild<QWidget*>("loki") );
    ui->tabWidget->setTabEnabled( lokiIndex, false);

    ui->lokiView->setModel( tosite_->loki() );
    ui->lokiView->horizontalHeader()->setSectionResizeMode(TositeLoki::KAYTTAJA, QHeaderView::Stretch);

    ui->hyvitaEnnakkoNappi->setVisible(false);

    paivitaLaskutustavat();
    laskutusTapaMuuttui();
    alustaMaksutavat();
}




void KantaLaskuDialogi::teeConnectit()
{
    connect( ui->asiakas, &AsiakasToimittajaValinta::muuttui, this, &KantaLaskuDialogi::asiakasMuuttui);
    connect( ui->asiakas, &AsiakasToimittajaValinta::valittu, this, &KantaLaskuDialogi::asiakasMuuttui);

    connect( ui->email, &QLineEdit::textChanged, this, &KantaLaskuDialogi::paivitaLaskutustavat);
    connect( ui->osoiteEdit, &QPlainTextEdit::textChanged, this, &KantaLaskuDialogi::paivitaLaskutustavat);
    connect( ui->laskutusCombo, &QComboBox::currentTextChanged, this, &KantaLaskuDialogi::laskutusTapaMuuttui);
    connect( ui->maksuCombo, &QComboBox::currentTextChanged, this, &KantaLaskuDialogi::maksuTapaMuuttui);
    connect( ui->valvontaCombo, &QComboBox::currentTextChanged, this, &KantaLaskuDialogi::valvontaMuuttui);
    connect( ui->tarkeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &KantaLaskuDialogi::paivitaViiteRivi);

    connect( ui->laskuPvm, &KpDateEdit::dateChanged, this, &KantaLaskuDialogi::laskeEraPaiva);
    connect( ui->maksuaikaSpin, qOverload<int>(&QSpinBox::valueChanged), this, &KantaLaskuDialogi::laskeEraPaiva );
    connect( ui->eraDate, &KpDateEdit::dateChanged, this, &KantaLaskuDialogi::laskeMaksuaika);

    connect( ui->esikatseluNappi, &QPushButton::clicked, this, &KantaLaskuDialogi::naytaEsikatselu);
    connect( ui->luonnosNappi, &QPushButton::clicked, [this] { this->tallenna(Tosite::LUONNOS); } );
    connect( ui->tallennaNappi, &QPushButton::clicked, [this] { this->tallenna(Tosite::VALMISLASKU); } );
    connect( ui->valmisNappi, &QPushButton::clicked, [this] { this->tallenna(Tosite::LAHETETAAN); } );

    connect( ui->lokiView, &QTableView::clicked, this, &KantaLaskuDialogi::naytaLoki);
    connect( ui->ohjeNappi, &QPushButton::clicked, [this] { kp()->ohje( this->ohje() ); });

}

void KantaLaskuDialogi::alustaMaksutavat()
{
    ui->maksuCombo->clear();
    ui->maksuCombo->addItem(QIcon(":/pic/lasku.png"), tr("Lasku"), Lasku::LASKU);
    if( tosite()->tyyppi() == TositeTyyppi::MYYNTILASKU) {
        ui->maksuCombo->addItem(QIcon(":/pic/kateinen.png"), tr("Käteinen"), Lasku::KATEINEN);
        ui->maksuCombo->addItem(QIcon(":/pic/ennakkolasku.png"), tr("Ennakkolasku"), Lasku::ENNAKKOLASKU);
        ui->maksuCombo->addItem(QIcon(":/pic/suorite.png"), tr("Suoriteperusteinen lasku"), Lasku::SUORITEPERUSTE);

        ui->maksuCombo->addItem(QIcon(":/pic/kuu.svg"), tr("Kuukausittainen lasku"), Lasku::KUUKAUSITTAINEN);
    }
}

void KantaLaskuDialogi::paivitaValvonnat()
{
    if(paivitysKaynnissa_) return;
    paivitysKaynnissa_ = true;
    int nykyinen = ui->valvontaCombo->currentData().toInt();
    ui->valvontaCombo->clear();

    if( maksutapa() != Lasku::KUUKAUSITTAINEN)
        ui->valvontaCombo->addItem(QIcon(":/pic/lasku.png"), tr("Yksittäinen lasku"), Lasku::LASKUVALVONTA);

    if( !ladattuAsiakas_.isEmpty()) ui->valvontaCombo->addItem(QIcon(":/pic/mies.png"), tr("Asiakas"), Lasku::ASIAKAS);
    if( huoneistot_->rowCount()) ui->valvontaCombo->addItem(QIcon(":/pic/talo.png"), tr("Huoneisto"), Lasku::HUONEISTO);
    if( kp()->vakioViitteet()->rowCount()) ui->valvontaCombo->addItem(QIcon(":/pic/viivakoodi.png"), tr("Vakioviite"), Lasku::VAKIOVIITE);
    ui->valvontaCombo->addItem(QIcon(":/pic/eikaytossa.png"), tr("Valvomaton"), Lasku::VALVOMATON);

    int indeksi = ui->valvontaCombo->findData(nykyinen);
    ui->valvontaCombo->setCurrentIndex(indeksi > -1 ? indeksi : 0);

    paivitysKaynnissa_ = false;
    valvontaMuuttui();
}

void KantaLaskuDialogi::tositteelta()
{
    tositteeltaKaynnissa_ = true;
    const Lasku& lasku = tosite()->constLasku();
    if( tosite()->kumppani()) {
        ui->asiakas->set( tosite()->kumppani(), tosite()->kumppaninimi());
    } else {
        ui->osoiteEdit->setPlainText( lasku.osoite() );
        jatkaTositteelta();
    }

    int lokiIndex = ui->tabWidget->indexOf( ui->tabWidget->findChild<QWidget*>("loki") );
    ui->tabWidget->setTabEnabled( lokiIndex, tosite()->loki()->rowCount() );

    QVariantMap era = tosite()->viennit()->vienti(0).era();
    bool maksettu = (!era.isEmpty() && qAbs( era.value("saldo").toDouble()) < 1e-5);
    ui->maksettuCheck->setVisible(maksettu);
    if( maksettu ) {
        ui->infoLabel->setText( tr("Maksettu "));
        ui->infoLabel->setStyleSheet("color: green;");
    } else {
        for(int i=0; i < tosite()->loki()->rowCount(); i++) {
            const QModelIndex& lokissa = tosite()->loki()->index(i, 0);
            const int tila = lokissa.data(TositeLoki::TilaRooli).toInt();
            const QDateTime aika = lokissa.data(TositeLoki::AikaRooli).toDateTime();

            if( tila == Tosite::TOIMITETTULASKU) {
                ui->infoLabel->setText( tr("Toimitettu %1").arg( aika.toString("dd.MM.yyyy hh.mm") ));
                break;
            } else if( tila == Tosite::LAHETETTYLASKU) {
                ui->infoLabel->setText( tr("Lähetetty %1").arg( aika.toString("dd.MM.yyyy hh.mm") ));
                break;
            }
        }
    }
    paivitaViiteRivi();
}

void KantaLaskuDialogi::jatkaTositteelta()
{
    const Lasku& lasku = tosite()->constLasku();
    ui->email->setText( lasku.email());

    ui->maksuCombo->setCurrentIndex( lasku.maksutapa() );
    ui->laskutusCombo->setCurrentIndex( ui->laskutusCombo->findData( lasku.lahetystapa() ) );
    ui->kieliCombo->setCurrentIndex( ui->kieliCombo->findData( lasku.kieli() ) );    

    ui->valvontaCombo->setCurrentIndex( ui->valvontaCombo->findData( lasku.valvonta() ));


    ViiteNumero viite( lasku.viite() );
    if( viite.tyyppi() == ViiteNumero::VAKIOVIITE || viite.tyyppi() == ViiteNumero::HUONEISTO ) {
        QString yksviite = ui->tarkeCombo->model()->index(0,0).data(HuoneistoModel::ViiteRooli).toString();
        ui->tarkeCombo->setCurrentIndex( ui->tarkeCombo->findData( viite.viite(), HuoneistoModel::ViiteRooli ) );
    }

    ui->toimitusDate->setDate( lasku.toimituspvm() );
    ui->jaksoDate->setDate( lasku.jaksopvm() );
    ui->eraDate->setDate( lasku.erapvm() );
    laskeMaksuaika();
    ui->toistoErapaivaSpin->setValue( lasku.toistuvanErapaiva() ? lasku.toistuvanErapaiva() : 4);

    ui->viivkorkoSpin->setValue( lasku.viivastyskorko() );
    ui->asViiteEdit->setText( lasku.asiakasViite());
    ui->otsikkoEdit->setText( lasku.otsikko() );

    ui->tilaajaEdit->setText( lasku.tilaaja());
    ui->myyjaEdit->setText( lasku.myyja());
    ui->tilausPvm->setDate( lasku.tilausPvm());
    ui->tilausnumeroEdit->setText( lasku.tilausNumero());
    ui->huomautusaikaSpin->setValue( lasku.huomautusAika() );
    ui->sopimusNumeroEdit->setText( lasku.sopimusnumero());

    ui->lisatietoEdit->setPlainText( lasku.lisatiedot() );
    ui->erittelyTextEdit->setPlainText( lasku.erittely().join('\n') );

    ui->saateEdit->setPlainText( lasku.saate() );
    ui->saateMaksutiedotCheck->setChecked( lasku.saatteessaMaksutiedot());        

    tositteeltaKaynnissa_ = false;
    paivitaViiteRivi();
}

void KantaLaskuDialogi::tositteelle()
{
    tosite()->asetaPvm( maksutapa() == Lasku::SUORITEPERUSTE ?
                        ui->toimitusDate->date() :
                        ui->laskuPvm->date());

    tosite()->asetaKumppani( ladattuAsiakas_ );
    tosite()->lasku().setOsoite( ui->osoiteEdit->toPlainText() );
    tosite()->lasku().setEmail( ui->email->text() );

    tosite()->lasku().setMaksutapa( maksutapa() );
    tosite()->lasku().setLahetystapa( ui->laskutusCombo->currentData().toInt());
    tosite()->lasku().setKieli( ui->kieliCombo->currentData().toString() );

    ViiteNumero viite( tosite()->lasku().viite() );
    const int valvonta = ui->valvontaCombo->currentData().toInt();

    tosite()->lasku().setValvonta( valvonta );
    if( valvonta == Lasku::VAKIOVIITE  || valvonta == Lasku::HUONEISTO) {
        QString viitenumero = ui->tarkeCombo->currentData(HuoneistoModel::ViiteRooli).toString();
        viite = ViiteNumero(viitenumero);
    } else if ( valvonta == Lasku::ASIAKAS) {
        viite = ViiteNumero(ViiteNumero::ASIAKAS, ui->asiakas->id() );
    }

    tosite()->asetaViite( viite );
    tosite()->lasku().setViite( viite );

    tosite()->lasku().setLaskunpaiva( ui->laskuPvm->date());
    tosite()->lasku().setToimituspvm( ui->toimitusDate->date() );
    tosite()->lasku().setJaksopvm( ui->jaksoDate->date() );

    tosite()->asetaErapvm(maksutapa() != Lasku::KUUKAUSITTAINEN
                                   ? ui->eraDate->date()
                                   : QDate());
    tosite()->lasku().setErapaiva( tosite()->erapvm() );

    tosite()->lasku().setToistuvanErapaiva( maksutapa() == Lasku::KUUKAUSITTAINEN
                                            ? ui->toistoErapaivaSpin->value()
                                            : 0 );

    tosite()->lasku().setViivastyskorko( ui->viivkorkoSpin->value() );
    tosite()->lasku().setAsiakasViite( ui->asViiteEdit->text());

    tosite()->asetaOtsikko( ui->otsikkoEdit->text() );
    tosite()->lasku().setOtsikko(ui->otsikkoEdit->text() );

    tosite()->lasku().setTilaaja( ui->tilaajaEdit->text() );
    tosite()->lasku().setMyyja( ui->myyjaEdit->text() );
    tosite()->lasku().setTilausPvm( ui->tilausPvm->date() );
    tosite()->lasku().setTilausNumero( ui->tilausnumeroEdit->text());
    tosite()->lasku().setHuomautusAika( ui->huomautusaikaSpin->value());
    tosite()->lasku().setSopimusnumero( ui->sopimusNumeroEdit->text());

    tosite()->lasku().setLisatiedot( ui->lisatietoEdit->toPlainText());
    if( ui->erittelyTextEdit->toPlainText().length())
        tosite()->lasku().setErittely( ui->erittelyTextEdit->toPlainText().split("\n") );
    else
        tosite()->lasku().setErittely(QStringList());

    tosite()->lasku().setSaate( ui->saateEdit->toPlainText() );
    tosite()->lasku().setSaatteessaMaksutiedot( ui->saateMaksutiedotCheck->isChecked() );

}

void KantaLaskuDialogi::asiakasMuuttui()
{
    const int asiakasId = ui->asiakas->nimi().isEmpty() ? 0 :  ui->asiakas->id();
    ui->osoiteEdit->setEnabled( asiakasId == 0);
    if( asiakasId && asiakasId != asiakasId_) {
        KpKysely *kysely = kpk( QString("/kumppanit/%1").arg(asiakasId) );
        connect( kysely, &KpKysely::vastaus, this, &KantaLaskuDialogi::taytaAsiakasTiedot);
        kysely->kysy();
    } else if( !asiakasId && asiakasId_) {
        ladattuAsiakas_.clear();
        ui->osoiteEdit->clear();
        ui->email->clear();
        paivitaLaskutustavat();
        emit alvTunnusVaihtui(QString());
    }
    asiakasId_ = asiakasId;
}

void KantaLaskuDialogi::taytaAsiakasTiedot(QVariant *data)
{
    QVariantMap map = data->toMap();
    ladattuAsiakas_ = map;

    ui->osoiteEdit->setPlainText( MaaModel::instanssi()->muotoiltuOsoite(map));

    ui->email->setText( map.value("email").toString());
    ui->kieliCombo->setCurrentIndex(ui->kieliCombo->findData(map.value("kieli","FI").toString()));

    paivitaLaskutustavat();
    const int laskutapaIndeksi = map.contains("laskutapa") ? ui->laskutusCombo->findData(map.value("laskutapa", -1)) : -1;
    if( laskutapaIndeksi > -1)
        ui->laskutusCombo->setCurrentIndex(laskutapaIndeksi);

    if( map.contains("maksuaika")) {
        ui->maksuaikaSpin->setValue(map.value("maksuaika").toInt());
    }

    if( map.value("alvtunnus").toString().isEmpty())
        // Yksityishenkilön viivästyskorko on peruskorko + 7 %
        ui->viivkorkoSpin->setValue( kp()->asetus("LaskuPeruskorko").toDouble() + 7.0 );
    else
        // Yrityksen viivästyskorko on peruskorko + 8 %
        ui->viivkorkoSpin->setValue( kp()->asetus("LaskuPeruskorko").toDouble() + 8.0 );

    if( tositteeltaKaynnissa_ )
        jatkaTositteelta();

    emit alvTunnusVaihtui( asiakkaanAlvTunnus() );
}

void KantaLaskuDialogi::paivitaLaskutustavat()
{
    if(paivitysKaynnissa_) {
        return;
    }
    paivitysKaynnissa_ = true;
    int nykyinen = ui->laskutusCombo->currentData().toInt();
    ui->laskutusCombo->clear();

    ui->laskutusCombo->addItem( QIcon(":/pic/tulosta.png"), tr("Tulosta lasku"), Lasku::TULOSTETTAVA);

    // Voidaan postittaa vain jos osoite asiakasrekisterissä
    if( ladattuAsiakas_.value("nimi").toString().length() > 1 &&
        ladattuAsiakas_.value("osoite").toString().length() > 1 &&
        ladattuAsiakas_.value("postinumero").toString().length() > 1 &&
        ladattuAsiakas_.value("kaupunki").toString() > 1 )
        ui->laskutusCombo->addItem( QIcon(":/pic/mail.png"), tr("Postita lasku"), Lasku::POSTITUS);

    if( kp()->asetukset()->luku("FinvoiceKaytossa") &&
        ladattuAsiakas_.value("ovt").toString().length() > 9 &&
        ladattuAsiakas_.value("operaattori").toString().length() > 4 )
        ui->laskutusCombo->addItem( QIcon(":/pic/verkkolasku.png"), tr("Verkkolasku"), Lasku::VERKKOLASKU);

    QRegularExpression emailRe(R"(^.*@.*\.\w+$)");
    if( emailRe.match( ui->email->text()).hasMatch() )
            ui->laskutusCombo->addItem(QIcon(":/pic/email.png"), tr("Lähetä sähköpostilla"), Lasku::SAHKOPOSTI);
    ui->laskutusCombo->addItem( QIcon(":/pic/pdf.png"), tr("Tallenna pdf-tiedostoon"), Lasku::PDF);
    ui->laskutusCombo->addItem( QIcon(":/pic/tyhja.png"), tr("Ei tulosteta"), Lasku::EITULOSTETA);

    int indeksi =   ui->laskutusCombo->findData(nykyinen);
    if( indeksi < 0) {
        ui->laskutusCombo->setCurrentIndex(0);
    } else {
        ui->laskutusCombo->setCurrentIndex(indeksi);
    }

    paivitysKaynnissa_ = false;
    laskutusTapaMuuttui();
    maksuTapaMuuttui();

}

void KantaLaskuDialogi::laskutusTapaMuuttui()
{
    int laskutustapa = ui->laskutusCombo->currentData().toInt();
    if( laskutustapa == Lasku::SAHKOPOSTI)
    {
        ui->valmisNappi->setText( tr("Tallenna ja lähetä sähköpostilla"));
        ui->valmisNappi->setIcon(QIcon(":/pic/email.png"));

    } else if( laskutustapa == Lasku::PDF) {
        ui->valmisNappi->setText( tr("Tallenna ja toimita"));
        ui->valmisNappi->setIcon(QIcon(":/pic/pdf.png"));
    } else if( laskutustapa == Lasku::EITULOSTETA) {
        ui->valmisNappi->setText( tr("Tallenna reskontraan"));
        ui->valmisNappi->setIcon(QIcon(":/pic/ok.png"));
    } else if( laskutustapa == Lasku::POSTITUS){
        ui->valmisNappi->setText( tr("Tallenna ja postita"));
        ui->valmisNappi->setIcon(QIcon(":/pic/mail.png"));
    } else if( laskutustapa == Lasku::VERKKOLASKU) {
        ui->valmisNappi->setText(tr("Tallenna ja lähetä"));
        ui->valmisNappi->setIcon(QIcon(":/pic/verkkolasku.png"));
    } else {
        ui->valmisNappi->setText( tr("Tallenna ja tulosta"));
        ui->valmisNappi->setIcon(QIcon(":/pic/tulosta.png"));
    }

    int saateIndex = ui->tabWidget->indexOf( ui->tabWidget->findChild<QWidget*>("saate") );
    ui->tabWidget->setTabEnabled( saateIndex, laskutustapa == Lasku::SAHKOPOSTI || !ui->laskutusCombo->isVisible());
}

void KantaLaskuDialogi::maksuTapaMuuttui()
{
    int maksutapa = ui->maksuCombo->currentData().toInt();

    ui->valvontaLabel->setVisible( maksutapa == Lasku::LASKU || maksutapa == Lasku::KUUKAUSITTAINEN);
    ui->valvontaCombo->setVisible( maksutapa == Lasku::LASKU || maksutapa == Lasku::KUUKAUSITTAINEN);
    valvontaMuuttui();


    ui->eraLabel->setVisible( maksutapa != Lasku::KATEINEN );
    ui->eraDate->setVisible( maksutapa != Lasku::KATEINEN && maksutapa != Lasku::KUUKAUSITTAINEN );

    ui->maksuaikaLabel->setVisible( maksutapa != Lasku::KATEINEN && maksutapa != Lasku::KUUKAUSITTAINEN );
    ui->maksuaikaSpin->setVisible( maksutapa != Lasku::KATEINEN && maksutapa != Lasku::KUUKAUSITTAINEN );


    ui->toistoErapaivaSpin->setVisible( maksutapa == Lasku::KUUKAUSITTAINEN);
    ui->viivkorkoLabel->setVisible( maksutapa != Lasku::KATEINEN );
    ui->viivkorkoSpin->setVisible( maksutapa != Lasku::KATEINEN );

    tosite()->rivit()->asetaEnnakkolasku(this->ui->maksuCombo->currentData().toInt() == Lasku::ENNAKKOLASKU);

    ui->toimituspvmLabel->setVisible(maksutapa != Lasku::ENNAKKOLASKU);
    ui->toimitusDate->setVisible(maksutapa != Lasku::ENNAKKOLASKU);
    ui->jaksoViivaLabel->setVisible(maksutapa != Lasku::ENNAKKOLASKU);
    ui->jaksoDate->setVisible( maksutapa != Lasku::ENNAKKOLASKU);

    ui->toimituspvmLabel->setText( maksutapa == Lasku::KUUKAUSITTAINEN ? tr("Laskut ajalla") : tr("Toimituspäivä") );
    paivitaValvonnat();
}

void KantaLaskuDialogi::valvontaMuuttui()
{
    if( paivitysKaynnissa_ ) return;

    bool valvontakaytossa = maksutapa() != Lasku::SUORITEPERUSTE && maksutapa() != Lasku::KATEINEN;

    const int valvonta = valvontakaytossa ? ui->valvontaCombo->currentData().toInt() : Lasku::LASKUVALVONTA;
    ui->tarkeCombo->setVisible( valvonta == Lasku::HUONEISTO || valvonta == Lasku::VAKIOVIITE );
    if( valvonta == Lasku::VAKIOVIITE && ui->tarkeCombo->model() != kp()->vakioViitteet())
        ui->tarkeCombo->setModel( kp()->vakioViitteet() );
    else if( valvonta == Lasku::HUONEISTO && ui->tarkeCombo->model() != huoneistot_)
        ui->tarkeCombo->setModel( huoneistot_ );

    paivitaViiteRivi();
}


void KantaLaskuDialogi::paivitaViiteRivi()
{
    ViiteNumero viite( tosite()->viite() );
    const int valvonta = ui->valvontaCombo->isVisible() ? ui->valvontaCombo->currentData().toInt() : Lasku::LASKUVALVONTA;
    if( valvonta == Lasku::ASIAKAS && !ladattuAsiakas_.isEmpty()) {
        viite = ViiteNumero(ViiteNumero::ASIAKAS, ladattuAsiakas_.value("id").toInt());
    } else if( valvonta == Lasku::VAKIOVIITE || valvonta == Lasku::HUONEISTO) {
        viite = ViiteNumero( ui->tarkeCombo->currentData(VakioViiteModel::ViiteRooli).toString() );
    }

    ui->rivitView->setColumnHidden(TositeRivit::TILI, valvonta == Lasku::VAKIOVIITE);

    ui->viiteText->setVisible( viite.tyyppi());

    ui->eiViitettaLabel->setVisible(  viite.tyyppi() == ViiteNumero::VIRHEELLINEN );
    ui->viiteText->setText(viite.valeilla());
}

void KantaLaskuDialogi::laskeEraPaiva()
{
    if( paivitysKaynnissa_ ) return;
    paivitysKaynnissa_ = true;

    QDate pvm = ui->laskuPvm->date().addDays(ui->maksuaikaSpin->value());
    ui->eraDate->setDate( Lasku::oikaiseErapaiva(pvm) );

    paivitysKaynnissa_ = false;
}

void KantaLaskuDialogi::laskeMaksuaika()
{
    if( paivitysKaynnissa_ ) return;
    paivitysKaynnissa_ = true;

    ui->maksuaikaSpin->setValue( ui->laskuPvm->date().daysTo( ui->eraDate->date() ) );

    paivitysKaynnissa_ = false;
}

void KantaLaskuDialogi::naytaLoki()
{
    NaytinIkkuna *naytin = new NaytinIkkuna();
    QVariant var = ui->lokiView->currentIndex().data(Qt::UserRole);

    QString data = QString::fromUtf8( QJsonDocument::fromVariant(var).toJson(QJsonDocument::Indented) );
    naytin->nayta(data);
}

void KantaLaskuDialogi::naytaEsikatselu()
{
    tositteelle();
    esikatsele();
}

bool KantaLaskuDialogi::tarkasta()
{
    return true;
}

void KantaLaskuDialogi::salliTallennus(bool sallinta)
{
    ui->valmisNappi->setEnabled(sallinta);
    ui->tallennaNappi->setEnabled(sallinta);
}

QString KantaLaskuDialogi::otsikko() const
{
    return tr("Lasku %1").arg(tosite_->lasku().numero());
}