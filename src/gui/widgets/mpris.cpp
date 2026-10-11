/*************************************************************************
 * This file is part of tuna
 * git.vrsal.xyz/alex/tuna
 * Copyright 2023 univrsal <uni@vrsal.xyz>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *************************************************************************/

#include "mpris.hpp"
#include "../../query/mpris_source.hpp"
#include "../../util/config.hpp"
#include "../../util/constants.hpp"
#include "../../util/utility.hpp"
#include "ui_mpris.h"
#include <QClipboard>
#include <QFile>
#include <QGuiApplication>

// Inside a flatpak the session bus is filtered, so players are only reachable
// if the sandbox was granted access to their bus names. Returns the app id if
// we're sandboxed without that access, otherwise an empty string.
static QString sandbox_blocking_mpris()
{
    QFile info("/.flatpak-info");
    if (!info.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QString app_id, section;
    while (!info.atEnd()) {
        auto line = QString::fromUtf8(info.readLine()).trimmed();
        if (line.startsWith('[')) {
            section = line;
            continue;
        }
        auto key = line.section('=', 0, 0);
        auto value = line.section('=', 1);
        if (section == "[Application]" && key == "name") {
            app_id = value;
        } else if (section == "[Context]" && key == "sockets") {
            if (value.split(';').contains("session-bus"))
                return {};
        } else if (section == "[Session Bus Policy]" && key.startsWith("org.mpris.MediaPlayer2")) {
            if (value == "talk" || value == "own")
                return {};
        }
    }
    return app_id;
}

mpris::mpris(QWidget* parent)
    : source_widget(parent)
    , ui(new Ui::mpris)
{
    ui->setupUi(this);

    auto app_id = sandbox_blocking_mpris();
    ui->sandbox_hint->setVisible(!app_id.isEmpty());
    ui->txt_sandbox_command->setText("flatpak override --user --talk-name='org.mpris.MediaPlayer2.*' " + app_id);
    connect(ui->btn_sandbox_copy, &QPushButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(ui->txt_sandbox_command->text());
    });
}

mpris::~mpris()
{
    delete ui;
}

void mpris::load_settings()
{
    auto idx = ui->cb_player->findData(utf8_to_qt(CGET_STR(CFG_MPRIS_PLAYER)));
    if (idx >= 0)
        ui->cb_player->setCurrentIndex(idx);
}

void mpris::save_settings()
{
    CSET_STR(CFG_MPRIS_PLAYER, qt_to_utf8(ui->cb_player->currentData().toString()));
}

void mpris::tick()
{
    auto mpris_src = music_sources::get<mpris_source>(S_SOURCE_MPRIS);
    if (mpris_src) {
        std::scoped_lock lock(mpris_src->m_internal_mutex);

        if (m_current_players != mpris_src->m_players) {
            m_current_players = mpris_src->m_players;
            auto current_player = ui->cb_player->currentData().toString();
            ui->cb_player->clear();

            for (auto const& player : mpris_src->m_players.keys())
                ui->cb_player->addItem(mpris_src->m_players[player], player);

            auto idx = ui->cb_player->findData(current_player);
            if (idx >= 0)
                ui->cb_player->setCurrentIndex(idx);
        }
    }
}

QComboBox* mpris::get_players()
{
    return ui->cb_player;
}
