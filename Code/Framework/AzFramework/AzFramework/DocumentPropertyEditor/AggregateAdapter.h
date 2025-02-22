/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/containers/map.h>
#include <AzCore/std/containers/vector.h>
#include <AzFramework/DocumentPropertyEditor/AdapterBuilder.h>
#include <AzFramework/DocumentPropertyEditor/DocumentAdapter.h>

namespace AZ::DocumentPropertyEditor
{
    //! class to allow multi-edits of row-based DPE adapters
    class RowAggregateAdapter : public DocumentAdapter
    {
    public:
        RowAggregateAdapter();
        virtual ~RowAggregateAdapter();

        void AddAdapter(DocumentAdapterPtr sourceAdapter);
        void RemoveAdapter(DocumentAdapterPtr sourceAdapter);
        void ClearAdapters();

        static bool IsRow(const Dom::Value& domValue);

    protected:
        struct AggregateNode
        {
            bool HasEntryForAdapter(size_t adapterIndex);
            Dom::Path GetPathForAdapter(size_t adapterIndex);
            void AddEntry(size_t adapterIndex, size_t pathEntryIndex, bool matchesOtherEntries);
            size_t EntryCount();

            static constexpr size_t InvalidEntry = size_t(-1);
            AZStd::vector<size_t> m_pathEntries; // per-adapter DOM index represented by AggregateNode

            bool m_allEntriesMatch = true;

            // track the last frame when this node changed state for easy and efficient patch generation
            unsigned int m_lastUpdateFrame = 0;

            // per-adapter mapping of DOM index to child
            AZStd::vector<AZStd::map<size_t, AggregateNode*>> m_pathIndexToChildMaps;

            AggregateNode* m_parent = nullptr;
            AZStd::vector<AZStd::unique_ptr<AggregateNode>> m_childRows; // ordered by primary adapter
        };

        // virtual function to generate an aggregate row that represents all the matching Dom::Values with in this node
        virtual Dom::Value GenerateAggregateRow(AggregateNode* matchingNode) = 0;

        // pure virtual to generate a "values differ" row that is appropriate for this type of AggregateAdapter
        // mismatchNode is provided so the row presented can include information from individual mismatched Dom::Values, if desired
        virtual Dom::Value GenerateValuesDifferRow(AggregateNode* mismatchNode) = 0;

        // pure virtual to determine if the row value from one adapter should be
        // considered the same aggregate row as a value from another adapter
        virtual bool SameRow(const Dom::Value& newRow, const Dom::Value& existingRow) = 0;

        // pure virtual to determine if two row values match such that they can be edited by one PropertyHandler
        virtual bool ValuesMatch(const Dom::Value& left, const Dom::Value& right) = 0;

        // message handlers for all owned adapters
        void HandleAdapterReset(DocumentAdapterPtr adapter);
        void HandleDomChange(DocumentAdapterPtr adapter, const Dom::Patch& patch);
        void HandleDomMessage(DocumentAdapterPtr adapter, const AZ::DocumentPropertyEditor::AdapterMessage& message, Dom::Value& value);

        // DocumentAdapter overrides
        Dom::Value GenerateContents() override;
        Dom::Value HandleMessage(const AdapterMessage& message) override;

        size_t GetIndexForAdapter(const DocumentAdapterPtr& adapter);
        AggregateNode* GetNodeAtAdapterPath(size_t adapterIndex, const Dom::Path& path);
        Dom::Value GetComparisonRow(AggregateNode* aggregateNode);

        //! gets the node at the given path relative to this adapter, if it exists
        AggregateNode* GetNodeAtPath(const Dom::Path& aggregatePath);
        Dom::Path GetPathForNode(AggregateNode* node); //!< returns the resultant path for this node if it exists, otherwise an empty path

        void PopulateNodesForAdapter(size_t adapterIndex);
        void PopulateChildren(size_t adapterIndex, const Dom::Value& parentValue, AggregateNode* parentNode);

        struct AdapterInfo
        {
            DocumentAdapter::ResetEvent::Handler resetHandler;
            ChangedEvent::Handler changedHandler;
            MessageEvent::Handler domMessageHandler;
            DocumentAdapterPtr adapter;
        };

        // all the adapters represented in this aggregate (multi-edit)
        AZStd::vector<AZStd::unique_ptr<AdapterInfo>> m_adapters;

        // potential rows always in the row order of the first adapter in m_adapters
        AZStd::unique_ptr<AggregateNode> m_rootNode;

        // monotonically increasing frame counter that increments whenever a source adapter gets an update
        unsigned int m_updateFrame = 0;

        AdapterBuilder m_builder;
    };

    class LabeledRowAggregateAdapter : public RowAggregateAdapter
    {
    protected:
        static AZStd::string_view GetFirstLabel(const Dom::Value& rowValue);

        Dom::Value GenerateAggregateRow(AggregateNode* matchingNode) override;
        Dom::Value GenerateValuesDifferRow(AggregateNode* mismatchNode) override;
        bool SameRow(const Dom::Value& newRow, const Dom::Value& existingRow) override;
        bool ValuesMatch(const Dom::Value& left, const Dom::Value& right) override;
    };

} // namespace AZ::DocumentPropertyEditor
