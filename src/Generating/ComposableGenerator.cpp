
// ComposableGenerator.cpp

// Implements the cComposableGenerator class representing the chunk generator that takes the composition approach to generating chunks

#include "Globals.h"

#include "ComposableGenerator.h"
#include "../World.h"
#include "../IniFile.h"
#include "../Root.h"

// Individual composed algorithms:
#include "BioGen.h"
#include "HeiGen.h"
#include "CompoGen.h"
#include "StructGen.h"
#include "FinishGen.h"

#include "CompoGenBiomal.h"

#include "CompositedHeiGen.h"

#include "Caves.h"
#include "DistortedHeightmap.h"
#include "DungeonRoomsFinisher.h"
#include "EndGen.h"
#include "MineShafts.h"
#include "Noise3DGenerator.h"
#include "Ravines.h"
#include "RoughRavines.h"
#include "VillageGen.h"
#include "PieceStructuresGen.h"





////////////////////////////////////////////////////////////////////////////////
// cTerrainCompositionGen:

cTerrainCompositionGenPtr cTerrainCompositionGen::CreateCompositionGen(cIniFile & a_IniFile, cBiomeGenPtr a_BiomeGen, cTerrainShapeGenPtr a_ShapeGen, int a_Seed)
{
	AString CompoGenName = a_IniFile.GetValueSet("Generator", "CompositionGen", "");
	if (CompoGenName.empty())
	{
		LOGWARN("[Generator] CompositionGen value not set in world.ini, using \"Biomal\".");
		CompoGenName = "Biomal";
	}

	// Compositor list is alpha-sorted
	cTerrainCompositionGenPtr res;
	if (NoCaseCompare(CompoGenName, "Biomal") == 0)
	{
		res = CreateCompoGenBiomal(a_Seed);
	}
	else if (NoCaseCompare(CompoGenName, "BiomalNoise3D") == 0)
	{
		// The composition that used to be provided with BiomalNoise3D is now provided by the Biomal compositor:
		res = CreateCompoGenBiomal(a_Seed);
	}
	else if (NoCaseCompare(CompoGenName, "Classic") == 0)
	{
		res = std::make_shared<cCompoGenClassic>();
	}
	else if (NoCaseCompare(CompoGenName, "DebugBiomes") == 0)
	{
		res = std::make_shared<cCompoGenDebugBiomes>();
	}
	else if (NoCaseCompare(CompoGenName, "DistortedHeightmap") == 0)
	{
		// The composition that used to be provided with DistortedHeightmap is now provided by the Biomal compositor:
		res = CreateCompoGenBiomal(a_Seed);
	}
	else if (NoCaseCompare(CompoGenName, "End") == 0)
	{
		res = std::make_shared<cEndGen>(a_Seed);
	}
	else if (NoCaseCompare(CompoGenName, "Nether") == 0)
	{
		res = std::make_shared<cCompoGenNether>(a_Seed);
	}
	else if (NoCaseCompare(CompoGenName, "Noise3D") == 0)
	{
		// The composition that used to be provided with Noise3D is now provided by the Biomal compositor:
		res = CreateCompoGenBiomal(a_Seed);
	}
	else if (NoCaseCompare(CompoGenName, "SameBlock") == 0)
	{
		res = std::make_shared<cCompoGenSameBlock>();
	}
	else
	{
		LOGWARN("Unknown CompositionGen \"%s\", using \"Biomal\" instead.", CompoGenName.c_str());
		a_IniFile.SetValue("Generator", "CompositionGen", "Biomal");
		return CreateCompositionGen(a_IniFile, a_BiomeGen, a_ShapeGen, a_Seed);
	}
	ASSERT(res != nullptr);

	// Read the settings from the ini file:
	res->InitializeCompoGen(a_IniFile);

	return cTerrainCompositionGenPtr(res);
}





////////////////////////////////////////////////////////////////////////////////
// cComposableGenerator:

cComposableGenerator::cComposableGenerator(cChunkGenerator & a_ChunkGenerator) :
	super(a_ChunkGenerator),
	m_BiomeGen(),
	m_ShapeGen(),
	m_CompositionGen()
{
}





void cComposableGenerator::Initialize(cIniFile & a_IniFile)
{
	super::Initialize(a_IniFile);

	InitBiomeGen(a_IniFile);
	InitShapeGen(a_IniFile);
	InitCompositionGen(a_IniFile);
	InitFinishGens(a_IniFile);
}





void cComposableGenerator::GenerateBiomes(int a_ChunkX, int a_ChunkZ, cChunkDef::BiomeMap & a_BiomeMap)
{
	if (m_BiomeGen != nullptr)  // Quick fix for generator deinitializing before the world storage finishes loading
	{
		m_BiomeGen->GenBiomes(a_ChunkX, a_ChunkZ, a_BiomeMap);
	}
}





void cComposableGenerator::DoGenerate(int a_ChunkX, int a_ChunkZ, cChunkDesc & a_ChunkDesc)
{
	if (a_ChunkDesc.IsUsingDefaultBiomes())
	{
		m_BiomeGen->GenBiomes(a_ChunkX, a_ChunkZ, a_ChunkDesc.GetBiomeMap());
	}

	cChunkDesc::Shape shape;
	if (a_ChunkDesc.IsUsingDefaultHeight())
	{
		m_ShapeGen->GenShape(a_ChunkX, a_ChunkZ, shape);
		a_ChunkDesc.SetHeightFromShape(shape);
	}
	else
	{
		// Convert the heightmap in a_ChunkDesc into shape:
		a_ChunkDesc.GetShapeFromHeight(shape);
	}

	bool ShouldUpdateHeightmap = false;
	if (a_ChunkDesc.IsUsingDefaultComposition())
	{
		m_CompositionGen->ComposeTerrain(a_ChunkDesc, shape);
	}

	if (a_ChunkDesc.IsUsingDefaultFinish())
	{
		for (cFinishGenList::iterator itr = m_FinishGens.begin(); itr != m_FinishGens.end(); ++itr)
		{
			(*itr)->GenFinish(a_ChunkDesc);
		}  // for itr - m_FinishGens[]
		ShouldUpdateHeightmap = true;
	}

	if (ShouldUpdateHeightmap)
	{
		a_ChunkDesc.UpdateHeightmap();
	}
}





void cComposableGenerator::InitBiomeGen(cIniFile & a_IniFile)
{
	bool CacheOffByDefault = false;
	m_BiomeGen = cBiomeGen::CreateBiomeGen(a_IniFile, m_ChunkGenerator.GetSeed(), CacheOffByDefault);

	// Add a cache, if requested:
	// The default is 16 * 128 caches, which is 2 MiB of RAM. Reasonable, for the amount of work this is saving.
	int CacheSize = a_IniFile.GetValueSetI("Generator", "BiomeGenCacheSize", CacheOffByDefault ? 0 : 16);
	if (CacheSize <= 0)
	{
		return;
	}
	int MultiCacheLength = a_IniFile.GetValueSetI("Generator", "BiomeGenMultiCacheLength", 128);
	if (CacheSize < 4)
	{
		LOGWARNING("Biomegen cache size set too low, would hurt performance instead of helping. Increasing from %d to %d",
			CacheSize, 4
		);
		CacheSize = 4;
	}
	LOGD("Using a cache for biomegen of size %d.", CacheSize);
	if (MultiCacheLength > 0)
	{
		LOGD("Enabling multicache for biomegen of length %d.", MultiCacheLength);
		m_BiomeGen = std::make_shared<cBioGenMulticache>(m_BiomeGen, static_cast<size_t>(CacheSize), static_cast<size_t>(MultiCacheLength));
	}
	else
	{
		m_BiomeGen = std::make_shared<cBioGenCache>(m_BiomeGen, static_cast<size_t>(CacheSize));
	}
}





void cComposableGenerator::InitShapeGen(cIniFile & a_IniFile)
{
	bool CacheOffByDefault = false;
	m_ShapeGen = cTerrainShapeGen::CreateShapeGen(a_IniFile, m_BiomeGen, m_ChunkGenerator.GetSeed(), CacheOffByDefault);

	/*
	// TODO
	// Add a cache, if requested:
	int CacheSize = a_IniFile.GetValueSetI("Generator", "ShapeGenCacheSize", CacheOffByDefault ? 0 : 64);
	if (CacheSize > 0)
	{
		if (CacheSize < 4)
		{
			LOGWARNING("Heightgen cache size set too low, would hurt performance instead of helping. Increasing from %d to %d",
				CacheSize, 4
			);
			CacheSize = 4;
		}
		LOGD("Using a cache for Heightgen of size %d.", CacheSize);
		m_HeightGen = cTerrainHeightGenPtr(new cHeiGenCache(m_HeightGen, CacheSize));
	}
	*/
}





void cComposableGenerator::InitCompositionGen(cIniFile & a_IniFile)
{
	m_CompositionGen = cTerrainCompositionGen::CreateCompositionGen(a_IniFile, m_BiomeGen, m_ShapeGen, m_ChunkGenerator.GetSeed());

	// Add a cache over the composition generator:
	// Even a cache of size 1 is useful due to the CompositedHeiGen cache after us doing re-composition on its misses
	int CompoGenCacheSize = a_IniFile.GetValueSetI("Generator", "CompositionGenCacheSize", 64);
	if (CompoGenCacheSize > 0)
	{
		m_CompositionGen = std::make_shared<cCompoGenCache>(m_CompositionGen, CompoGenCacheSize);
	}

	// Create a cache of the composited heightmaps, so that finishers may use it:
	m_CompositedHeightCache = std::make_shared<cHeiGenMultiCache>(std::make_shared<cCompositedHeiGen>(m_BiomeGen, m_ShapeGen, m_CompositionGen), 16, 128);
	// 128 subcaches of depth 16 each = 0.5 MiB of RAM. Acceptable, for the amount of work this saves.
}





void cComposableGenerator::InitFinishGens(cIniFile & a_IniFile)
{
	int Seed = m_ChunkGenerator.GetSeed();
	eDimension Dimension = StringToDimension(a_IniFile.GetValue("General", "Dimension", "Overworld"));
	auto seaLevel = a_IniFile.GetValueI("Generator", "SeaLevel");

	AString Finishers = a_IniFile.GetValueSet("Generator", "Finishers", "");

	// Create all requested finishers:
	AStringVector Str = StringSplitAndTrim(Finishers, ",");
	for (AStringVector::const_iterator itr = Str.begin(); itr != Str.end(); ++itr)
	{
		auto split = StringSplitAndTrim(*itr, ":");
		if (split.empty())
		{
			continue;
		}
		const auto & finisher = split[0];
		// Finishers, alpha-sorted:
		if (NoCaseCompare(finisher, "Animals") == 0)
		{
			m_FinishGens.push_back(cFinishGenPtr(new cFinishGenPassiveMobs(Seed, a_IniFile, Dimension)));
		}
		else if (NoCaseCompare(finisher, "BottomLava") == 0)
		{
			int DefaultBottomLavaLevel = (Dimension == dimNether) ? 30 : 10;
			int BottomLavaLevel = a_IniFile.GetValueSetI("Generator", "BottomLavaLevel", DefaultBottomLavaLevel);
			m_FinishGens.push_back(cFinishGenPtr(new cFinishGenBottomLava(BottomLavaLevel)));
		}
		else if (NoCaseCompare(finisher, "DeadBushes") == 0)
		{
			// A list with all the allowed biomes.
			cFinishGenSingleTopBlock::BiomeList AllowedBiomes;
			AllowedBiomes.push_back(biDesert);
			AllowedBiomes.push_back(biDesertHills);
			AllowedBiomes.push_back(biDesertM);
			AllowedBiomes.push_back(biMesa);
			AllowedBiomes.push_back(biMesaBryce);
			AllowedBiomes.push_back(biMesaPlateau);
			AllowedBiomes.push_back(biMesaPlateauF);
			AllowedBiomes.push_back(biMesaPlateauFM);
			AllowedBiomes.push_back(biMesaPlateauM);

			// A list with all the allowed blocks that can be below the dead bush.
			cFinishGenSingleTopBlock::BlockList AllowedBlocks;
			AllowedBlocks.push_back(E_BLOCK_SAND);
			AllowedBlocks.push_back(E_BLOCK_HARDENED_CLAY);
			AllowedBlocks.push_back(E_BLOCK_STAINED_CLAY);

			m_FinishGens.push_back(cFinishGenPtr(new cFinishGenSingleTopBlock(Seed, E_BLOCK_DEAD_BUSH, AllowedBiomes, 2, AllowedBlocks)));
		}
		else if (NoCaseCompare(finisher, "DirectOverhangs") == 0)
		{
			m_FinishGens.push_back(cFinishGenPtr(new cStructGenDirectOverhangs(Seed)));
		}
		else if (NoCaseCompare(finisher, "DirtPockets") == 0)
		{
			auto gen = std::make_shared<cFinishGenOrePockets>(Seed + 1, cFinishGenOrePockets::DefaultNaturalPatches());
			if (gen->Initialize(a_IniFile, "DirtPockets"))
			{
				m_FinishGens.push_back(gen);
			}
		}
		else if (NoCaseCompare(finisher, "DistortedMembraneOverhangs") == 0)
		{
			m_FinishGens.push_back(cFinishGenPtr(new cStructGenDistortedMembraneOverhangs(Seed)));
		}
		else if (NoCaseCompare(finisher, "DualRidgeCaves") == 0)
		{
			float Threshold = static_cast<float>(a_IniFile.GetValueSetF("Generator", "DualRidgeCavesThreshold", 0.3));
			m_FinishGens.push_back(cFinishGenPtr(new cStructGenDualRidgeCaves(Seed, Threshold)));
		}
		else if (NoCaseCompare(finisher, "DungeonRooms") == 0)
		{
			int     GridSize      = a_IniFile.GetValueSetI("Generator", "DungeonRoomsGridSize", 48);
			int     MaxSize       = a_IniFile.GetValueSetI("Generator", "DungeonRoomsMaxSize", 7);
			int     MinSize       = a_IniFile.GetValueSetI("Generator", "DungeonRoomsMinSize", 5);
			AString HeightDistrib = a_IniFile.GetValueSet ("Generator", "DungeonRoomsHeightDistrib", "0, 0; 10, 10; 11, 500; 40, 500; 60, 40; 90, 1");
			m_FinishGens.push_back(cFinishGenPtr(new cDungeonRoomsFinisher(m_ShapeGen, Seed, GridSize, MaxSize, MinSize, HeightDistrib)));
		}
		else if (NoCaseCompare(finisher, "GlowStone") == 0)
		{
			m_FinishGens.push_back(cFinishGenPtr(new cFinishGenGlowStone(Seed)));
		}
		else if (NoCaseCompare(finisher, "Ice") == 0)
		{
			m_FinishGens.push_back(cFinishGenPtr(new cFinishGenIce));
		}
		else if (NoCaseCompare(finisher, "LavaLakes") == 0)
		{
			int Probability = a_IniFile.GetValueSetI("Generator", "LavaLakesProbability", 10);
			m_FinishGens.push_back(cFinishGenPtr(new cStructGenLakes(Seed * 5 + 16873, E_BLOCK_STATIONARY_LAVA, m_ShapeGen, Probability)));
		}
		else if (NoCaseCompare(finisher, "LavaSprings") == 0)
		{
			m_FinishGens.push_back(cFinishGenPtr(new cFinishGenFluidSprings(Seed, E_BLOCK_LAVA, a_IniFile, Dimension)));
		}
		else if (NoCaseCompare(finisher, "Lilypads") == 0)
		{
			// A list with all the allowed biomes.
			cFinishGenSingleTopBlock::BiomeList AllowedBiomes;
			AllowedBiomes.push_back(biSwampland);
			AllowedBiomes.push_back(biSwamplandM);

			// A list with all the allowed blocks that can be below the lilypad.
			cFinishGenSingleTopBlock::BlockList AllowedBlocks;
			AllowedBlocks.push_back(E_BLOCK_WATER);
			AllowedBlocks.push_back(E_BLOCK_STATIONARY_WATER);

			m_FinishGens.push_back(cFinishGenPtr(new cFinishGenSingleTopBlock(Seed, E_BLOCK_LILY_PAD, AllowedBiomes, 4, AllowedBlocks)));
		}
		else if (NoCaseCompare(finisher, "MarbleCaves") == 0)
		{
			m_FinishGens.push_back(cFinishGenPtr(new cStructGenMarbleCaves(Seed)));
		}
		else if (NoCaseCompare(finisher, "MineShafts") == 0)
		{
			int GridSize        = a_IniFile.GetValueSetI("Generator", "MineShaftsGridSize",        512);
			int MaxOffset       = a_IniFile.GetValueSetI("Generator", "MineShaftsMaxOffset",       256);
			int MaxSystemSize   = a_IniFile.GetValueSetI("Generator", "MineShaftsMaxSystemSize",   160);
			int ChanceCorridor  = a_IniFile.GetValueSetI("Generator", "MineShaftsChanceCorridor",  600);
			int ChanceCrossing  = a_IniFile.GetValueSetI("Generator", "MineShaftsChanceCrossing",  200);
			int ChanceStaircase = a_IniFile.GetValueSetI("Generator", "MineShaftsChanceStaircase", 200);
			m_FinishGens.push_back(cFinishGenPtr(new cStructGenMineShafts(
				Seed, GridSize, MaxOffset, MaxSystemSize,
				ChanceCorridor, ChanceCrossing, ChanceStaircase
			)));
		}
		else if (NoCaseCompare(finisher, "NaturalPatches") == 0)
		{
			m_FinishGens.push_back(std::make_shared<cFinishGenOreNests>(Seed + 1, cFinishGenOreNests::DefaultNaturalPatches()));
		}
		else if (NoCaseCompare(finisher, "NetherClumpFoliage") == 0)
		{
			m_FinishGens.push_back(cFinishGenPtr(new cFinishGenNetherClumpFoliage(Seed)));
		}
		else if (NoCaseCompare(*itr, "NetherForts") == 0)
		{
			LOGINFO("The NetherForts finisher is obsolete, you should use \"PieceStructures: NetherFort\" instead.");
			auto gen = std::make_shared<cPieceStructuresGen>(Seed);
			if (gen->Initialize("NetherFort", seaLevel, m_BiomeGen, m_CompositedHeightCache))
			{
				m_FinishGens.push_back(gen);
			}
		}
		else if (NoCaseCompare(finisher, "NetherOreNests") == 0)
		{
			m_FinishGens.push_back(std::make_shared<cFinishGenOreNests>(Seed + 2, cFinishGenOreNests::DefaultNetherOres()));
		}
		else if (NoCaseCompare(finisher, "OreNests") == 0)
		{
			m_FinishGens.push_back(std::make_shared<cFinishGenOreNests>(Seed + 3, cFinishGenOreNests::DefaultOverworldOres()));
		}
		else if (NoCaseCompare(finisher, "OrePockets") == 0)
		{
			auto gen = std::make_shared<cFinishGenOrePockets>(Seed + 2, cFinishGenOrePockets::DefaultOverworldOres());
			if (gen->Initialize(a_IniFile, "OrePockets"))
			{
				m_FinishGens.push_back(gen);
			}
		}
		else if (NoCaseCompare(finisher, "OverworldClumpFlowers") == 0)
		{
			AString flowers = a_IniFile.GetValueSet("Generator", "OverworldClumpFlowers", "Forest+-2+2,ForestHills+-3+2,FlowerForest; yellowflower,redflower,lilac,rosebush | plains+-2+1,SunflowerPlains; yellowflower,redflower,azurebluet,oxeyedaisy | SunflowerPlains+1+2; sunflower | FlowerForest+2+5; allium,redtulip,orangetulip,whitetulip,pinktulip,oxeyedaisy | Swampland,SwamplandM; brownmushroom,redmushroom,blueorchid");
			m_FinishGens.push_back(cFinishGenPtr(new cFinishGenClumpTopBlock(Seed, cFinishGenClumpTopBlock::ParseConfigurationString(flowers))));
		}
		else if (NoCaseCompare(finisher, "PieceStructures") == 0)
		{
			if (split.size() < 2)
			{
				LOGWARNING("The PieceStructures generator needs the structures to use. Example: \"PieceStructures: NetherFort\".");
				continue;
			}

			auto gen = std::make_shared<cPieceStructuresGen>(Seed);
			if (gen->Initialize(split[1], seaLevel, m_BiomeGen, m_CompositedHeightCache))
			{
				m_FinishGens.push_back(gen);
			}
		}
		else if (NoCaseCompare(finisher, "PreSimulator") == 0)
		{
			// Load the settings
			bool PreSimulateFallingBlocks = a_IniFile.GetValueSetB("Generator", "PreSimulatorFallingBlocks", true);
			bool PreSimulateWater         = a_IniFile.GetValueSetB("Generator", "PreSimulatorWater", true);
			bool PreSimulateLava          = a_IniFile.GetValueSetB("Generator", "PreSimulatorLava", true);

			m_FinishGens.push_back(cFinishGenPtr(new cFinishGenPreSimulator(PreSimulateFallingBlocks, PreSimulateWater, PreSimulateLava)));
		}
		else if (NoCaseCompare(finisher, "RainbowRoads") == 0)
		{
			LOGINFO("The RainbowRoads finisher is obsolete, you should use \"PieceStructures: RainbowRoads\" instead.");
			auto gen = std::make_shared<cPieceStructuresGen>(Seed);
			if (gen->Initialize("RainbowRoads", seaLevel, m_BiomeGen, m_CompositedHeightCache))
			{
				m_FinishGens.push_back(gen);
			}
		}
		else if (NoCaseCompare(finisher, "Ravines") == 0)
		{
			m_FinishGens.push_back(cFinishGenPtr(new cStructGenRavines(Seed, 128)));
		}
		else if (NoCaseCompare(finisher, "RoughRavines") == 0)
		{
			int GridSize                  = a_IniFile.GetValueSetI("Generator", "RoughRavinesGridSize",              256);
			int MaxOffset                 = a_IniFile.GetValueSetI("Generator", "RoughRavinesMaxOffset",             128);
			int MaxSize                   = a_IniFile.GetValueSetI("Generator", "RoughRavinesMaxSize",               128);
			int MinSize                   = a_IniFile.GetValueSetI("Generator", "RoughRavinesMinSize",                64);
			double MaxCenterWidth         = a_IniFile.GetValueSetF("Generator", "RoughRavinesMaxCenterWidth",          8);
			double MinCenterWidth         = a_IniFile.GetValueSetF("Generator", "RoughRavinesMinCenterWidth",          2);
			double MaxRoughness           = a_IniFile.GetValueSetF("Generator", "RoughRavinesMaxRoughness",            0.2);
			double MinRoughness           = a_IniFile.GetValueSetF("Generator", "RoughRavinesMinRoughness",            0.05);
			double MaxFloorHeightEdge     = a_IniFile.GetValueSetF("Generator", "RoughRavinesMaxFloorHeightEdge",      8);
			double MinFloorHeightEdge     = a_IniFile.GetValueSetF("Generator", "RoughRavinesMinFloorHeightEdge",     30);
			double MaxFloorHeightCenter   = a_IniFile.GetValueSetF("Generator", "RoughRavinesMaxFloorHeightCenter",   20);
			double MinFloorHeightCenter   = a_IniFile.GetValueSetF("Generator", "RoughRavinesMinFloorHeightCenter",    6);
			double MaxCeilingHeightEdge   = a_IniFile.GetValueSetF("Generator", "RoughRavinesMaxCeilingHeightEdge",   56);
			double MinCeilingHeightEdge   = a_IniFile.GetValueSetF("Generator", "RoughRavinesMinCeilingHeightEdge",   38);
			double MaxCeilingHeightCenter = a_IniFile.GetValueSetF("Generator", "RoughRavinesMaxCeilingHeightCenter", 58);
			double MinCeilingHeightCenter = a_IniFile.GetValueSetF("Generator", "RoughRavinesMinCeilingHeightCenter", 36);
			m_FinishGens.push_back(cFinishGenPtr(new cRoughRavines(
				Seed, MaxSize, MinSize,
				static_cast<float>(MaxCenterWidth),
				static_cast<float>(MinCenterWidth),
				static_cast<float>(MaxRoughness),
				static_cast<float>(MinRoughness),
				static_cast<float>(MaxFloorHeightEdge),
				static_cast<float>(MinFloorHeightEdge),
				static_cast<float>(MaxFloorHeightCenter),
				static_cast<float>(MinFloorHeightCenter),
				static_cast<float>(MaxCeilingHeightEdge),
				static_cast<float>(MinCeilingHeightEdge),
				static_cast<float>(MaxCeilingHeightCenter),
				static_cast<float>(MinCeilingHeightCenter),
				GridSize, MaxOffset
			)));
		}
		else if (NoCaseCompare(finisher, "SoulsandRims") == 0)
		{
			m_FinishGens.push_back(cFinishGenPtr(new cFinishGenSoulsandRims(Seed)));
		}
		else if (NoCaseCompare(finisher, "Snow") == 0)
		{
			m_FinishGens.push_back(cFinishGenPtr(new cFinishGenSnow));
		}
		else if (NoCaseCompare(finisher, "SprinkleFoliage") == 0)
		{
			m_FinishGens.push_back(cFinishGenPtr(new cFinishGenSprinkleFoliage(Seed)));
		}
		else if (NoCaseCompare(finisher, "TallGrass") == 0)
		{
			m_FinishGens.push_back(cFinishGenPtr(new cFinishGenTallGrass(Seed)));
		}
		else if (NoCaseCompare(finisher, "Trees") == 0)
		{
			m_FinishGens.push_back(cFinishGenPtr(new cStructGenTrees(Seed, m_BiomeGen, m_ShapeGen, m_CompositionGen)));
		}
		else if (NoCaseCompare(finisher, "UnderwaterBases") == 0)
		{
			LOGINFO("The UnderwaterBases finisher is obsolete, you should use \"PieceStructures: UnderwaterBases\" instead.");
			auto gen = std::make_shared<cPieceStructuresGen>(Seed);
			if (gen->Initialize("UnderwaterBases", seaLevel, m_BiomeGen, m_CompositedHeightCache))
			{
				m_FinishGens.push_back(gen);
			}
		}
		else if (NoCaseCompare(finisher, "Villages") == 0)
		{
			int GridSize   = a_IniFile.GetValueSetI("Generator", "VillageGridSize",  384);
			int MaxOffset  = a_IniFile.GetValueSetI("Generator", "VillageMaxOffset", 128);
			int MaxDepth   = a_IniFile.GetValueSetI("Generator", "VillageMaxDepth",    2);
			int MaxSize    = a_IniFile.GetValueSetI("Generator", "VillageMaxSize",   128);
			int MinDensity = a_IniFile.GetValueSetI("Generator", "VillageMinDensity", 50);
			int MaxDensity = a_IniFile.GetValueSetI("Generator", "VillageMaxDensity", 80);
			AString PrefabList = a_IniFile.GetValueSet("Generator", "VillagePrefabs", "PlainsVillage, SandVillage");
			auto Prefabs = StringSplitAndTrim(PrefabList, ",");
			m_FinishGens.push_back(std::make_shared<cVillageGen>(Seed, GridSize, MaxOffset, MaxDepth, MaxSize, MinDensity, MaxDensity, m_BiomeGen, m_CompositedHeightCache, seaLevel, Prefabs));
		}
		else if (NoCaseCompare(finisher, "Vines") == 0)
		{
			int Level = a_IniFile.GetValueSetI("Generator", "VinesLevel", 40);
			m_FinishGens.push_back(std::make_shared<cFinishGenVines>(Seed, Level));
		}
		else if (NoCaseCompare(finisher, "WaterLakes") == 0)
		{
			int Probability = a_IniFile.GetValueSetI("Generator", "WaterLakesProbability", 25);
			m_FinishGens.push_back(cFinishGenPtr(new cStructGenLakes(Seed * 3 + 652, E_BLOCK_STATIONARY_WATER, m_ShapeGen, Probability)));
		}
		else if (NoCaseCompare(finisher, "WaterSprings") == 0)
		{
			m_FinishGens.push_back(cFinishGenPtr(new cFinishGenFluidSprings(Seed, E_BLOCK_WATER, a_IniFile, Dimension)));
		}
		else if (NoCaseCompare(finisher, "WormNestCaves") == 0)
		{
			int Size      = a_IniFile.GetValueSetI("Generator", "WormNestCavesSize", 64);
			int Grid      = a_IniFile.GetValueSetI("Generator", "WormNestCavesGrid", 96);
			int MaxOffset = a_IniFile.GetValueSetI("Generator", "WormNestMaxOffset", 32);
			m_FinishGens.push_back(cFinishGenPtr(new cStructGenWormNestCaves(Seed, Size, Grid, MaxOffset)));
		}
		else
		{
			LOGWARNING("Unknown Finisher in the [Generator] section: \"%s\". Ignoring.", finisher.c_str());
		}
	}  // for itr - Str[]
}




