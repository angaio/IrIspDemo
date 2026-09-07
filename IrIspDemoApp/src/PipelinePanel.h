/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
/******************************************************************************
* File        : PipelinePanel.h
* Description : Editor for the processing pipeline.
*
* Holds an editable list of nodes (each node = one algorithm + its parameters +
* an enabled flag), lets the user add / remove / reorder them, and rebuilds a
* parameter panel for the selected node from the library's descriptors. Emits
* modelChanged() on any edit; the window pushes the model to the worker.
******************************************************************************/
#ifndef ___PipelinePanel_h___
#define ___PipelinePanel_h___

#include <QWidget>
#include <QVector>
#include <QMap>

class QListWidget;
class QListWidgetItem;
class QVBoxLayout;
class QGroupBox;

struct NodeModel
{
    int algoId = 0;
    bool enabled = true;
    QMap<QString, int> params;
};

class PipelinePanel : public QWidget
{
    Q_OBJECT
public:
    explicit PipelinePanel(QWidget *parent = nullptr);

    const QVector<NodeModel> &model() const { return m_model; }
    void seedDefault();     // a sensible starting chain

signals:
    void modelChanged();

private slots:
    void onAdd();
    void onRemove();
    void onUp();
    void onDown();
    void onSelectionChanged();
    void onItemChanged(QListWidgetItem *item);

private:
    void refreshList();
    void rebuildParams();
    NodeModel makeNode(int algoId) const;
    int  currentRow() const;

    QListWidget *m_list = nullptr;
    QGroupBox   *m_paramBox = nullptr;
    QVBoxLayout *m_paramLayout = nullptr;
    QVector<NodeModel> m_model;
    bool m_guard = false;
};

#endif
