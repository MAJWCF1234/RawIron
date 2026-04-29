---
tags:
  - rawiron
  - engine
  - assets
  - content
  - models
---

# Declarative Model Definition

## Overview

The Declarative Model Definition is a data-driven asset format used to describe static or semi-static models in structured form. Rather than defining geometry and materials directly in code, this format stores model composition as serialized data that can be loaded and turned into renderable objects by a generic model-construction system.

Its purpose is to support reusable, inspectable, engine-readable model assets that can be authored, transformed, or generated without requiring dedicated procedural code modules for each object.

## Purpose

Raw Iron is being built around modular and data-oriented workflows. In that environment, not every asset should require a custom source module. Many objects are better represented as declarative data that describes:

- mesh parts
- transforms
- hierarchy
- primitive composition
- material assignments
- configurable parameters

The Declarative Model Definition exists to provide a standard path for assets that can be represented in structured data instead of handwritten procedural logic.

## Core Function

A declarative model file stores the information needed for a generic loader or builder to reconstruct a model at load time. Depending on the engine’s evolving asset schema, this may include:

- named parts
- transforms and pivots
- primitive or mesh references
- material identifiers
- parent-child relationships
- optional metadata and tags

A generic construction path can then interpret this data and create the corresponding runtime object.

## Role in Raw Iron

Within Raw Iron, this artifact should be treated as a data-driven model asset format. It belongs to the content layer and serves as one of the standard ways for assets to enter the runtime.

## Native Implementation

Native types and JSON helpers live in `RawIron.Content`:

- Header: `Source/RawIron.Content/include/RawIron/Content/DeclarativeModelDefinition.h`
- Namespace: `ri::content`

The current serialized shape includes `formatVersion`, `modelId`, and a `parts` array. Each part carries identifiers (`name`, `parentName`, `meshId`, `materialId`), `translation` / `rotation` / `scale` objects, and optional `tags`.

Connecting this format to scene objects or rendering is a separate integration step (generic builder / loader); the definition itself is the authored interchange.

## Related Notes

- [[Asset Extraction Inventory]]
- [[06 Content Assembly]]
- [[03 Assets/File Formats|File Format Decisions]]
- [[Library Layers]]
