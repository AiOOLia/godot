#include "register_types.h"

#include "usd_document.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/import/3d/resource_importer_scene.h"

class EditorSceneFormatImporterUSD : public EditorSceneFormatImporter {
	GDCLASS(EditorSceneFormatImporterUSD, EditorSceneFormatImporter);

public:
	void get_extensions(List<String> *r_extensions) const override {
		r_extensions->push_back("usd");
		r_extensions->push_back("usda");
		r_extensions->push_back("usdc");
	}

	Node *import_scene(const String &p_path, uint32_t p_flags, const HashMap<StringName, Variant> &p_options,
			List<String> *r_missing_deps, Error *r_err = nullptr) override {
		return USDDocument::import_scene_internal(p_path, true, r_err);
	}
};

static void _editor_init() {
	Ref<EditorSceneFormatImporterUSD> importer;
	importer.instantiate();
	ResourceImporterScene::add_scene_importer(importer);
}
#endif

void initialize_usd_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(USDDocument);
	}
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_CLASS(EditorSceneFormatImporterUSD);
		EditorNode::add_init_callback(_editor_init);
	}
#endif
}

void uninitialize_usd_module(ModuleInitializationLevel p_level) {
}
