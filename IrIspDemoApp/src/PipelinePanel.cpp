/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
#include "PipelinePanel.h"
#include "AlgoNames.h"
#include "IrProcess.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QString>

#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QToolButton>
#include <QMenu>
#include <QLabel>
#include <QSlider>
#include <QStyle>

PipelinePanel::PipelinePanel(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    QLabel *title = new QLabel(QStringLiteral("Pipeline"), this);
    title->setObjectName(QStringLiteral("panelTitle"));
    root->addWidget(title);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_list, &QListWidget::currentRowChanged, this, &PipelinePanel::onSelectionChanged);
    connect(m_list, &QListWidget::itemChanged, this, &PipelinePanel::onItemChanged);
    root->addWidget(m_list, 1);

    // Node toolbar.
    QHBoxLayout *bar = new QHBoxLayout;
    bar->setSpacing(4);
    auto mkBtn = [&](QStyle::StandardPixmap ic, const QString &tip) {
        QToolButton *b = new QToolButton(this);
        b->setIcon(style()->standardIcon(ic));
        b->setToolTip(tip);
        b->setAutoRaise(true);
        return b;
    };
    QToolButton *addBtn = mkBtn(QStyle::SP_FileDialogNewFolder, QStringLiteral("Add node"));
    addBtn->setPopupMode(QToolButton::InstantPopup);
    QMenu *addMenu = new QMenu(addBtn);
    for (int stage = 0; stage <= IR_STAGE_ENHANCE; ++stage) {
        QMenu *sub = addMenu->addMenu(algo::stageName(stage));
        for (int i = 0; i < IrCatalog::count(); ++i) {
            int id = IrCatalog::idAt(i);
            if (IrCatalog::stageOf(id) != stage) continue;
            QAction *a = sub->addAction(algo::name(id));
            a->setData(id);
            connect(a, &QAction::triggered, this, [this, id] {
                m_model.append(makeNode(id));
                refreshList();
                m_list->setCurrentRow(m_model.size() - 1);
                emit modelChanged();
            });
        }
    }
    addBtn->setMenu(addMenu);

    QToolButton *rmBtn = mkBtn(QStyle::SP_TrashIcon, QStringLiteral("Remove node"));
    QToolButton *upBtn = mkBtn(QStyle::SP_ArrowUp, QStringLiteral("Move up"));
    QToolButton *dnBtn = mkBtn(QStyle::SP_ArrowDown, QStringLiteral("Move down"));
    connect(rmBtn, &QToolButton::clicked, this, &PipelinePanel::onRemove);
    connect(upBtn, &QToolButton::clicked, this, &PipelinePanel::onUp);
    connect(dnBtn, &QToolButton::clicked, this, &PipelinePanel::onDown);
    bar->addWidget(addBtn);
    bar->addWidget(rmBtn);
    bar->addStretch(1);
    bar->addWidget(upBtn);
    bar->addWidget(dnBtn);
    root->addLayout(bar);

    // Parameter area for the selected node.
    m_paramBox = new QGroupBox(QStringLiteral("Parameters"), this);
    m_paramLayout = new QVBoxLayout(m_paramBox);
    m_paramLayout->setContentsMargins(8, 8, 8, 8);
    root->addWidget(m_paramBox);
}

NodeModel PipelinePanel::makeNode(int algoId) const
{
    NodeModel n;
    n.algoId = algoId;
    n.enabled = true;
    for (int i = 0; i < IrCatalog::paramCount(algoId); ++i) {
        IrParamDesc d = IrCatalog::paramOf(algoId, i);
        n.params.insert(QString::fromLatin1(d.key), d.defV);
    }
    return n;
}

void PipelinePanel::seedDefault()
{
    m_model.clear();
    m_model.append(makeNode(30));   // DDE enhancement, a good default
    refreshList();
    m_list->setCurrentRow(0);
    emit modelChanged();
}

void PipelinePanel::saveModel(const QString &path) const
{
    QJsonArray arr;
    for (const NodeModel &n : m_model) {
        QJsonObject o;
        o.insert(QStringLiteral("algoId"), n.algoId);
        o.insert(QStringLiteral("enabled"), n.enabled);
        QJsonObject pj;
        for (auto it = n.params.constBegin(); it != n.params.constEnd(); ++it)
            pj.insert(it.key(), it.value());
        o.insert(QStringLiteral("params"), pj);
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("nodes"), arr);
    QFileInfo(path).absoluteDir().mkpath(QStringLiteral("."));
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool PipelinePanel::loadModel(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) return false;
    const QJsonArray arr = doc.object().value(QStringLiteral("nodes")).toArray();
    QVector<NodeModel> loaded;
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        const int id = o.value(QStringLiteral("algoId")).toInt(-1);
        bool known = false;
        for (int i = 0; i < IrCatalog::count(); ++i) if (IrCatalog::idAt(i) == id) { known = true; break; }
        if (!known) continue;
        NodeModel n = makeNode(id);                 // seed all current defaults
        n.enabled = o.value(QStringLiteral("enabled")).toBool(true);
        const QJsonObject pj = o.value(QStringLiteral("params")).toObject();
        for (auto it = pj.constBegin(); it != pj.constEnd(); ++it)
            if (n.params.contains(it.key())) n.params[it.key()] = it.value().toInt();
        loaded.append(n);
    }
    if (loaded.isEmpty()) return false;
    m_model = loaded;
    refreshList();
    m_list->setCurrentRow(0);
    emit modelChanged();
    return true;
}

int PipelinePanel::currentRow() const { return m_list->currentRow(); }

void PipelinePanel::refreshList()
{
    m_guard = true;
    const int keep = m_list->currentRow();
    m_list->clear();
    for (int i = 0; i < m_model.size(); ++i) {
        const NodeModel &n = m_model[i];
        auto *it = new QListWidgetItem(
            QStringLiteral("%1.  %2   [%3]")
                .arg(i + 1).arg(algo::name(n.algoId))
                .arg(algo::stageName(IrCatalog::stageOf(n.algoId))));
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(n.enabled ? Qt::Checked : Qt::Unchecked);
        m_list->addItem(it);
    }
    if (keep >= 0 && keep < m_model.size()) m_list->setCurrentRow(keep);
    m_guard = false;
    rebuildParams();
}

void PipelinePanel::rebuildParams()
{
    // Clear the parameter area.
    QLayoutItem *child;
    while ((child = m_paramLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    const int row = currentRow();
    if (row < 0 || row >= m_model.size()) {
        QLabel *hint = new QLabel(QStringLiteral("Select a node to edit its parameters."), m_paramBox);
        hint->setWordWrap(true);
        m_paramLayout->addWidget(hint);
        return;
    }
    NodeModel &n = m_model[row];
    const int algoId = n.algoId;

    QFormLayout *form = new QFormLayout;
    for (int i = 0; i < IrCatalog::paramCount(algoId); ++i) {
        IrParamDesc d = IrCatalog::paramOf(algoId, i);
        const QString key = QString::fromLatin1(d.key);

        QSlider *sl = new QSlider(Qt::Horizontal, m_paramBox);
        sl->setRange(d.minV, d.maxV);
        sl->setValue(n.params.value(key, d.defV));
        QLabel *val = new QLabel(QString::number(sl->value()), m_paramBox);
        val->setMinimumWidth(34);
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        connect(sl, &QSlider::valueChanged, this, [this, row, key, val](int v) {
            if (row < 0 || row >= m_model.size()) return;
            m_model[row].params[key] = v;
            val->setText(QString::number(v));
            emit modelChanged();
        });

        QWidget *rowW = new QWidget(m_paramBox);
        QHBoxLayout *hl = new QHBoxLayout(rowW);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(sl, 1);
        hl->addWidget(val, 0);
        form->addRow(QString::fromLatin1(d.label), rowW);
    }
    QWidget *host = new QWidget(m_paramBox);
    host->setLayout(form);
    m_paramLayout->addWidget(host);
}

void PipelinePanel::onSelectionChanged() { rebuildParams(); }

void PipelinePanel::onItemChanged(QListWidgetItem *item)
{
    if (m_guard) return;
    int row = m_list->row(item);
    if (row < 0 || row >= m_model.size()) return;
    bool en = (item->checkState() == Qt::Checked);
    if (m_model[row].enabled != en) {
        m_model[row].enabled = en;
        emit modelChanged();
    }
}

void PipelinePanel::onAdd() {}   // handled by the add menu

void PipelinePanel::onRemove()
{
    int row = currentRow();
    if (row < 0 || row >= m_model.size()) return;
    m_model.remove(row);
    refreshList();
    emit modelChanged();
}

void PipelinePanel::onUp()
{
    int row = currentRow();
    if (row <= 0) return;
    m_model.swapItemsAt(row, row - 1);
    refreshList();
    m_list->setCurrentRow(row - 1);
    emit modelChanged();
}

void PipelinePanel::onDown()
{
    int row = currentRow();
    if (row < 0 || row >= m_model.size() - 1) return;
    m_model.swapItemsAt(row, row + 1);
    refreshList();
    m_list->setCurrentRow(row + 1);
    emit modelChanged();
}
