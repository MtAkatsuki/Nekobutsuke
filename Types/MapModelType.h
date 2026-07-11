#pragma once
#include <string>

// =========================================================
// マップモデルの種類
// =========================================================
enum class MapModelType {
	FLOOR,            // 床
	WALL,             // 壁
	TRAP,             // トラップ

	// --- 家具・プロップ (長方形・大型物件) ---
	PROP_SOFA_YOKO,   // ソファ（横向き 3x1）
	PROP_SOFA_TATE,   // ソファ（縦向き 1x3）
	PROP_BOOKSHELF,   // 本棚（横 2x1）
	PROP_CATTOWER,    // キャットタワー（横 2x1）
	PROP_TABLE        // テーブル（横 3x2）
};

// =========================================================
// プロップ定義テーブル
// CSVトークン・メッシュ名・占有サイズ・通行可否を一元管理する。
// 新しい家具の追加はここに1行 + GameScene のモデル登録に1行のみ。
// ※ FLOOR は特殊扱い（テーブル外、通行可・床メッシュ直指定）
// =========================================================
struct PropDef {
	MapModelType type;
	const char* csvToken;   // レベルCSV上の識別子
	const char* meshName;   // MeshManager 登録名
	int          sizeX;      // 占有幅（X軸）
	int          sizeZ;      // 占有奥行き（Z軸）
	bool         walkable;   // 通行可否
};

inline constexpr PropDef PROP_DEFS[] = {
	// type                          csvToken       meshName            W  D  walkable
	{ MapModelType::WALL,            "W",           "prop_plane_mesh",  1, 1, false },
	{ MapModelType::TRAP,            "T",           "trap_mesh",        1, 1, true  },
	{ MapModelType::PROP_SOFA_YOKO,  "W_Y_SOFA",    "sofa_yoko_mesh",   3, 1, false },
	{ MapModelType::PROP_SOFA_TATE,  "W_T_SOFA",    "sofa_tate_mesh",   1, 3, false },
	{ MapModelType::PROP_BOOKSHELF,  "W_BOOKSHELF", "bookshelf_mesh",   2, 1, false },
	{ MapModelType::PROP_CATTOWER,   "W_CATTOWER",  "cattower_mesh",    2, 1, false },
	{ MapModelType::PROP_TABLE,      "W_TABLE",     "table_mesh",       3, 2, false },
};

// タイプから定義を検索（未登録タイプ＝FLOOR等は nullptr）
inline const PropDef* FindPropDef(MapModelType type) {
	for (const auto& def : PROP_DEFS) {
		if (def.type == type) return &def;
	}
	return nullptr;
}

// CSVトークンから定義を検索（未知トークンは nullptr）
inline const PropDef* FindPropDefByToken(const std::string& token) {
	for (const auto& def : PROP_DEFS) {
		if (token == def.csvToken) return &def;
	}
	return nullptr;
}