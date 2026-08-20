// Fill out your copyright notice in the Description page of Project Settings.


#include "ScrapbookingFunctionLibrary.h"
#include "HAL/PlatformApplicationMisc.h"
#include "GameDataTypes.h"

// Math utils
#include "Math/Box2D.h"
#include "Math/UnrealMathUtility.h"

// Proc mesh
#include "ProceduralMeshComponent.h"

// GeometryCore
#include "Curve/GeneralPolygon2.h"
#include "Polygon2.h"
#include "Curve/PolygonIntersectionUtils.h"

// GeometryScript
#include "UDynamicMesh.h"
#include "MeshBoundaryLoops.h"
#include "Generators/SweepGenerator.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/MeshIndexMappings.h"
#include "DynamicMeshEditor.h"
#include "ConversionUtils/SceneComponentToDynamicMesh.h"
#include "Components/SceneComponent.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Operations/MeshBoolean.h"
#include "DynamicMesh/MeshTransforms.h"
#include "Selections/MeshConnectedComponents.h"
#include "MeshQueries.h"

//actual geometry script
#include "GeometryScript/MeshBooleanFunctions.h"

// editor
#include "Misc/StringOutputDevice.h"
#include "ImageUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Texture2D.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <commdlg.h>
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

// ============================
//*****************************
// Debug stuff
//*****************************
// ============================
#include "DrawDebugHelpers.h"

// Helper to map from a mesh local space to a normalized UV-like value 0-1 thats in its local bounds
static FVector2D Static_MeshLocalToNormalized(const FVector3d& MeshLocalPos, const FBox& MeshLocalBoundsBox)
{
    FVector BoxSize = MeshLocalBoundsBox.GetSize();

    if (BoxSize.X <= 0.0f || BoxSize.Y <= 0.0f) return FVector2D::ZeroVector;

    double NormX = (MeshLocalPos.X - MeshLocalBoundsBox.Min.X) / BoxSize.X;
    double NormY = (MeshLocalPos.Y - MeshLocalBoundsBox.Min.Y) / BoxSize.Y;

    return FVector2D(NormX, NormY);
}

// Helper to map from the normalized UV space to world space
static FVector Static_NormalizedToWorld(const FVector2D& UVPos, const FBox& MeshLocalBoundsBox, const FTransform& MeshTransform)
{
    FVector BoxSize = MeshLocalBoundsBox.GetSize();
    FVector MeshLocalPos;

    MeshLocalPos.X = MeshLocalBoundsBox.Min.X + (UVPos.X * BoxSize.X);
    MeshLocalPos.Y = MeshLocalBoundsBox.Min.Y + (UVPos.Y * BoxSize.Y);
    MeshLocalPos.Z = MeshLocalBoundsBox.Min.Z; // This doesn't super matter but lets just place it at teh min

    return MeshTransform.TransformPosition(MeshLocalPos);
}

// Helper to map from the local space of a mesh to the UV space of a DIFFERENT mesh (i.e. local space of a piece of evidence to the UV space of a page)
static FVector2D Static_MeshLocalToOtherMeshNormalizedUV(const FVector3d& StartingMeshLocalPos, const FTransform& StartingMeshTransform, const FTransform& EndingMeshTransform, const FBox& EndingMeshBoundsBox)
{
    // Starting Local Space -> World Space
    FVector3d WorldPos = StartingMeshTransform.TransformPosition(StartingMeshLocalPos);

    // World Space -> Ending Local Space
    FVector3d EndingLocalPos = EndingMeshTransform.InverseTransformPosition(WorldPos);

    // Ending Local Space -> UV Space
    return Static_MeshLocalToNormalized(EndingLocalPos, EndingMeshBoundsBox);
}

// Debug draw the page bounds (just sanity check)
static void Static_DebugDrawPageBounds(UWorld* World, const FTransform& PageTransform, const FBox& PageBox, float Duration)
{
    FVector WorldCenter = PageTransform.TransformPosition(PageBox.GetCenter());
    FVector LocalExtent = PageBox.GetExtent() * PageTransform.GetScale3D();

    DrawDebugBox(World, WorldCenter, LocalExtent, PageTransform.GetRotation(), FColor::White, false, Duration, 0, 1.5f);
}

// Debug draw page areas
static void Static_DebugDrawPageAreas(UWorld* World, const FScrapbookPage& Page, const FTransform& PageTransform, const FBox& PageBox, float Duration)
{
    for (const FScrapbookArea& Area : Page.Areas)
    {
        FVector2D TopLeftUV = Area.Position;
        FVector2D BottomRightUV = Area.Position + Area.Size;

        FVector TLWorld = Static_NormalizedToWorld(TopLeftUV, PageBox, PageTransform);
        FVector TRWorld = Static_NormalizedToWorld(FVector2D(BottomRightUV.X, TopLeftUV.Y), PageBox, PageTransform);
        FVector BRWorld = Static_NormalizedToWorld(BottomRightUV, PageBox, PageTransform);
        FVector BLWorld = Static_NormalizedToWorld(FVector2D(TopLeftUV.X, BottomRightUV.Y), PageBox, PageTransform);

        DrawDebugLine(World, TLWorld, TRWorld, FColor::Green, false, Duration, 0, 2.0f);
        DrawDebugLine(World, TRWorld, BRWorld, FColor::Green, false, Duration, 0, 2.0f);
        DrawDebugLine(World, BRWorld, BLWorld, FColor::Green, false, Duration, 0, 2.0f);
        DrawDebugLine(World, BLWorld, TLWorld, FColor::Green, false, Duration, 0, 2.0f);

        FString TraitName = Area.Trait.TypeName.ToString();
        DrawDebugString(World, TLWorld, *TraitName, nullptr, FColor::Green, Duration);
    }
}

// Debug draw the loops of a given dynamic mesh (helping me to view the result mesh after all the booleans finish when player cuts)
static void Static_DebugDrawBoundaryLoops(UWorld* World, UDynamicMesh* TargetMesh, const FTransform& TargetMeshTransform, const FTransform& PageTransform, const FBox& PageBox, float Duration)
{
    using namespace UE::Geometry;

    TargetMesh->ProcessMesh([&](const FDynamicMesh3& Mesh)
        {
            FMeshBoundaryLoops BoundaryLoops(&Mesh);

            for (const FEdgeLoop& Loop : BoundaryLoops.Loops)
            {
                TArray<FVector> ReconstructedWorldPoints;

                for (int32 VertID : Loop.Vertices)
                {
                    FVector3d LocalMeshPos = Mesh.GetVertex(VertID);

                    // Piece Local -> Page Normalized UV
                    FVector2D UV = Static_MeshLocalToOtherMeshNormalizedUV(LocalMeshPos, TargetMeshTransform, PageTransform, PageBox);

                    // Normalized UV -> World Space
                    FVector ReconstructedWorldPos = Static_NormalizedToWorld(UV, PageBox, PageTransform);
                    ReconstructedWorldPoints.Add(ReconstructedWorldPos);
                }

                // Draw connecting lines for the loop
                for (int32 i = 0; i < ReconstructedWorldPoints.Num(); ++i)
                {
                    FVector PtA = ReconstructedWorldPoints[i];
                    FVector PtB = ReconstructedWorldPoints[(i + 1) % ReconstructedWorldPoints.Num()];
                    DrawDebugLine(World, PtA, PtB, FColor::Cyan, false, Duration, 0, 2.5f);
                }
            }
        });
}

// I call this big guy from BP to just visualize all the page data - Specifically it helps me visually check that the space conversions were all correct
void UScrapbookingFunctionLibrary::DebugDrawPageNormalizationResult(
    const UObject* WorldContextObject,
    UDynamicMesh* EvidenceMesh,
    const FTransform& EvidenceMeshTransform,
    const FTransform& PageTransform,
    FBox PageBox,
    const FScrapbookPage& Page,
    float Duration)
{
    UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
    if (!World || !EvidenceMesh || PageBox.GetSize().X <= 0.0f) return;

    Static_DebugDrawPageBounds(World, PageTransform, PageBox, Duration);
    Static_DebugDrawPageAreas(World, Page, PageTransform, PageBox, Duration);
    Static_DebugDrawBoundaryLoops(World, EvidenceMesh, EvidenceMeshTransform, PageTransform, PageBox, Duration);
}

// Simple debug draw for a path, shows while cutting
void UScrapbookingFunctionLibrary::DebugDrawPath(const UObject* WorldContextObject, const TArray<FVector>& PathPoints)
{
    if (PathPoints.Num() < 2)
    {
        return;
    }

    UWorld* World = GEngine ? GEngine->GetWorldFromContextObjectChecked(WorldContextObject) : nullptr;
    if (!World)
    {
        return;
    }

    for (int32 i = 0; i < PathPoints.Num() - 1; ++i)
    {
        DrawDebugLine(
            World,
            PathPoints[i] + 5 * FVector::UpVector,
            PathPoints[i + 1] + 5 * FVector::UpVector,
            FColor::Green,
            false,  // Persistent
            0.0f,   // Lifetime: one frame
            0,      // Depth priority
            5.0f    // Thickness
        );
    }
}

// Simpler way to debug draw the page data
void UScrapbookingFunctionLibrary::DebugDrawPageAreas(
    const UObject* WorldContextObject,
    const FTransform& PageComponentTransform,
    const FVector& PageLocalMin,
    const FVector& PageLocalMax,
    const FScrapbookPage& Page)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObjectChecked(WorldContextObject) : nullptr;
    if (!World)
    {
        return;
    }

    const FVector LocalPageSize = PageLocalMax - PageLocalMin;

    for (const FScrapbookArea& Area : Page.Areas)
    {
        // 1. Calculate Area Center and Extent in 0-1 UV space
        FVector2D UVCenter = Area.Position + (Area.Size * 0.5f);
        FVector2D UVExtent = Area.Size * 0.5f;

        // 2. Map UV coordinates to Page Local Space
        // Assumes U -> Local X, V -> Local Y (adjust mapping if your mesh uses X/Z or Y/Z)
        FVector LocalCenter(
            PageLocalMin.X + (UVCenter.X * LocalPageSize.X),
            PageLocalMin.Y + (UVCenter.Y * LocalPageSize.Y),
            (PageLocalMin.Z + PageLocalMax.Z) * 0.5f // Center along Z
        );

        // Calculate Local Extent (half-size). 
        // We add a tiny Z thickness (e.g., 0.5f) so flat 2D planes draw clearly without Z-fighting.
        FVector LocalExtent(
            UVExtent.X * FMath::Abs(LocalPageSize.X),
            UVExtent.Y * FMath::Abs(LocalPageSize.Y),
            FMath::Max(0.5f, FMath::Abs(LocalPageSize.Z * 0.5f))
        );

        // 3. Transform Center, Rotation, and Scale into World Space
        FVector WorldCenter = PageComponentTransform.TransformPosition(LocalCenter);
        FQuat WorldRotation = PageComponentTransform.GetRotation();
        FVector WorldExtent = LocalExtent * PageComponentTransform.GetScale3D().GetAbs();

        // 4. Render Box in World Space
        DrawDebugBox(
            World,
            WorldCenter,
            WorldExtent,
            WorldRotation,
            FColor::Green, // Green line color
            false,         // bPersistentLines (false = temporary)
            0.0f,          // Lifetime in seconds (0.0f = single frame, ideal for tick debug)
            0,             // DepthPriority
            5.5f           // Line Thickness
        );

        // 5. Render area info 
        DrawDebugString(
            World,
            WorldCenter,
            FString::Printf(TEXT("%s:%d"), *Area.Trait.TypeName.ToString(), Area.Trait.Magnitude),
            nullptr,
            FColor::White,
            0.0f
        );
    }
}

// ============================
//*****************************
// Geometry stuff for gameplay
//*****************************
// ============================

// Quickly make a mesh section from the players cut path
void UScrapbookingFunctionLibrary::TriangulatePathPoints(
    const FTransform& SourceComponentTransform,
    const FTransform& ProcComponentTransform,
    const FVector& SourceLocalMin,
    const FVector& SourceLocalMax,
    const TArray<FVector>& PathPoints,
    TArray<FVector>& Vertices,
    TArray<int32>& Triangles,
    TArray<FVector>& Normals,
    TArray<FVector2D>& UV0)
{
    Vertices.Reset();
    Triangles.Reset();
    Normals.Reset();
    UV0.Reset();

    const int32 NumPoints = PathPoints.Num();

    if (NumPoints < 3)
    {
        return;
    }

    Vertices.Reserve(NumPoints);
    Normals.Reserve(NumPoints);
    UV0.Reserve(NumPoints);

    // Get size of source mesh
    const float SourceWidth = SourceLocalMax.X - SourceLocalMin.X;
    const float SourceHeight = SourceLocalMax.Y - SourceLocalMin.Y;

    // Convert world-space path points to component-local space.
    TArray<FVector> LocalPoints;
    LocalPoints.Reserve(NumPoints);

    // Fixed normal
    FVector Normal = ProcComponentTransform.InverseTransformVector(FVector::DownVector);

    for (const FVector& WorldPoint : PathPoints)
    {
        const FVector ProcLocalPoint = ProcComponentTransform.InverseTransformPosition(WorldPoint);

        LocalPoints.Add(ProcLocalPoint);

        Vertices.Add(ProcLocalPoint);

        Normals.Add(Normal);

        // UVs are based on source mesh space, not local. UV coords/size need to be converted from world to source mesh space
        const FVector SourceLocalPoint = SourceComponentTransform.InverseTransformPosition(WorldPoint);
        UV0.Add(FVector2D(
            (SourceLocalPoint.X - SourceLocalMin.X) / SourceWidth,
            (SourceLocalPoint.Y - SourceLocalMin.Y) / SourceHeight
        ));
    }

    // Ear clipping algorithm operates on the proc mesh local points
    TArray<int32> Remaining;
    Remaining.Reserve(NumPoints);
    for (int32 i = 0; i < NumPoints; ++i)
    {
        Remaining.Add(i);
    }

    auto Cross2D = [](const FVector& A,
        const FVector& B,
        const FVector& C) -> double
        {
            const double ABx = static_cast<double>(B.X) - A.X;
            const double ABy = static_cast<double>(B.Y) - A.Y;
            const double ACx = static_cast<double>(C.X) - A.X;
            const double ACy = static_cast<double>(C.Y) - A.Y;

            return ABx * ACy - ABy * ACx;
        };

    auto PointInTriangle = [&](const FVector& A,
        const FVector& B,
        const FVector& C,
        const FVector& P) -> bool
        {
            const double C1 = Cross2D(A, B, P);
            const double C2 = Cross2D(B, C, P);
            const double C3 = Cross2D(C, A, P);

            return C1 >= -SMALL_NUMBER &&
                C2 >= -SMALL_NUMBER &&
                C3 >= -SMALL_NUMBER;
        };

    while (Remaining.Num() > 3)
    {
        bool FoundEar = false;
        const int32 Count = Remaining.Num();

        for (int32 i = 0; i < Count; ++i)
        {
            const int32 PrevIndex =
                Remaining[(i - 1 + Count) % Count];

            const int32 CurrIndex =
                Remaining[i];

            const int32 NextIndex =
                Remaining[(i + 1) % Count];

            const FVector& A = LocalPoints[PrevIndex];
            const FVector& B = LocalPoints[CurrIndex];
            const FVector& C = LocalPoints[NextIndex];

            if (Cross2D(A, B, C) <= SMALL_NUMBER)
            {
                continue;
            }

            bool ContainsOtherPoint = false;

            for (int32 j = 0; j < Count; ++j)
            {
                const int32 TestIndex = Remaining[j];

                if (TestIndex == PrevIndex ||
                    TestIndex == CurrIndex ||
                    TestIndex == NextIndex)
                {
                    continue;
                }

                if (PointInTriangle(
                    A,
                    B,
                    C,
                    LocalPoints[TestIndex]))
                {
                    ContainsOtherPoint = true;
                    break;
                }
            }

            if (ContainsOtherPoint)
            {
                continue;
            }

            Triangles.Add(PrevIndex);
            Triangles.Add(CurrIndex);
            Triangles.Add(NextIndex);

            Remaining.RemoveAt(i);
            FoundEar = true;
            break;
        }

        if (!FoundEar)
        {
            Triangles.Reset();
            return;
        }
    }

    if (Remaining.Num() == 3)
    {
        Triangles.Add(Remaining[0]);
        Triangles.Add(Remaining[1]);
        Triangles.Add(Remaining[2]);
    }

    const FVector A = Vertices[Triangles[0]];
    const FVector B = Vertices[Triangles[1]];
    const FVector C = Vertices[Triangles[2]];

    const FVector FaceNormal =
        FVector::CrossProduct(B - A, C - A).GetSafeNormal();

    UE_LOG(LogTemp, Warning,
        TEXT("Face normal = %s"),
        *FaceNormal.ToString());
}

// This not only cchecks for a loop, but also discards any tail/trailing part of the path that isn't on the loop perimeter. Cleaning it up
bool UScrapbookingFunctionLibrary::CheckForPathIntersection(
    const TArray<FVector>& PathPoints,
    TArray<FVector>& LoopedPoints)
{
    const int32 NumPoints = PathPoints.Num();

    LoopedPoints.Empty();

    // Need at least 4 points to have a segment intersect
    // a non-adjacent previous segment.
    if (NumPoints < 4)
    {
        return false;
    }

    // NOTE: Assumes input points share the XY plane.

    // The newest segment is always the one that caused the
    // lasso to terminate.
    const int32 NewestSegmentIndex = NumPoints - 2;

    const FVector NewestStart(
        PathPoints[NewestSegmentIndex].X,
        PathPoints[NewestSegmentIndex].Y,
        0.0f);

    const FVector NewestEnd(
        PathPoints[NewestSegmentIndex + 1].X,
        PathPoints[NewestSegmentIndex + 1].Y,
        0.0f);

    // Search backwards through older segments.
    //
    // Start at NumPoints - 4 because:
    //
    //   Newest segment = N-2 -> N-1
    //   Adjacent segment = N-3 -> N-2  <-- skip
    //
    // Therefore the first valid segment is:
    //
    //   N-4 -> N-3
    //
    for (int32 j = NumPoints - 4; j >= 0; --j)
    {
        const FVector OldStart(
            PathPoints[j].X,
            PathPoints[j].Y,
            0.0f);

        const FVector OldEnd(
            PathPoints[j + 1].X,
            PathPoints[j + 1].Y,
            0.0f);

        FVector IntersectionPoint;

        if (FMath::SegmentIntersection2D(
            NewestStart,
            NewestEnd,
            OldStart,
            OldEnd,
            IntersectionPoint))
        {
            // Keep the portion of the path between the
            // old intersecting segment and the newest segment.
            LoopedPoints.Reserve(NumPoints - j);

            for (int32 k = j + 1; k <= NewestSegmentIndex; ++k)
            {
                LoopedPoints.Add(PathPoints[k]);
            }

            // I'm just assuming that the Z is the same for the whole path here
            IntersectionPoint.Z = PathPoints[j+1].Z;

            LoopedPoints.Add(IntersectionPoint);

            // normalize the winding
            if (CalculateSignedArea(LoopedPoints) < 0.0)
            {
                Algo::Reverse(LoopedPoints);
            }

            return true;
        }
    }

    return false;
}

// Just some math
float UScrapbookingFunctionLibrary::CalculateSignedArea(const TArray<FVector>& PathPoints)
{
    int32 NumPoints = PathPoints.Num();

    // A polygon must have at least 3 vertices to enclose an area
    if (NumPoints < 3)
    {
        return 0.0f;
    }

    float DoubleArea = 0.0f;

    // Shoelace algorithm: sum the cross products of adjacent vertex pairs
    for (int32 i = 0; i < NumPoints; ++i)
    {
        // Wrap around to the first point when reaching the last point
        int32 NextIndex = (i + 1) % NumPoints;

        const FVector& Current = PathPoints[i];
        const FVector& Next = PathPoints[NextIndex];

        DoubleArea += (Current.X * Next.Y) - (Next.X * Current.Y);
    }

    return DoubleArea * 0.5f;
}

float UScrapbookingFunctionLibrary::CalculateArea(const TArray<FVector>& PathPoints)
{
    return FMath::Abs(CalculateSignedArea(PathPoints));
}

static float CalculateArea2D(const TArray<FVector2D>& PathPoints)
{
    TArray<FVector> PathPoints3d;
    for (const auto& v : PathPoints)
        PathPoints3d.Add(FVector(v.X, v.Y, 0));
    return UScrapbookingFunctionLibrary::CalculateArea(PathPoints3d);
}

// Main gameplay function that takes the final evidence mesh (may contain holes, be concave, all sorts of nasty stuff)
// It tests the intersection of that evidence mesh with all areas on the page and grabs the traits from overlapping areas proportionally
// There's some visualization as well for debug purposes - The intersection of a page Area with the evidence mesh is shown in orange
// This uses some UE Geometry lib functions because they have good fast algos
// Note page areas store their bboxes in Normalized 0-1 UV space of the page mesh!! Must convert to same space before doing the intersection
void UScrapbookingFunctionLibrary::GetClippedTraitsFromMeshAndPage(
    const UObject* WorldContextObject,
    const FTransform& EvidenceMeshTransform,
    const FTransform& PageTransform,
    FBox PageSize,
    const FScrapbookPage& Page,
    UDynamicMesh* EvidenceMesh,
    bool DrawDebug,
    TArray<FEvidenceTrait>& ClippedTraits)
{
    ClippedTraits.Empty();

    // 1. Process the 3D Dynamic Mesh to extract 2D Boundary Loops in Page UV Space
    UE::Geometry::FGeneralPolygon2d EvidenceGenPoly;
    EvidenceMesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Mesh)
        {
            UE::Geometry::FMeshBoundaryLoops BoundaryLoops(&Mesh);
            if (BoundaryLoops.Loops.Num() == 0) return;

            int32 OuterLoopIdx = -1;
            double LargestArea = -1.0;
            TArray<UE::Geometry::FPolygon2d> ParsedPolygons;

            // Convert 3D boundary loops to 2D Polygons (projecting onto X/Y plane)
            for (int32 i = 0; i < BoundaryLoops.Loops.Num(); ++i)
            {
                UE::Geometry::FPolygon2d Poly2D;
                for (int32 VertID : BoundaryLoops.Loops[i].Vertices)
                {
                    FVector3d LocalMeshPos = Mesh.GetVertex(VertID);

                    // Piece Local -> World Space
                    FVector3d WorldPos = EvidenceMeshTransform.TransformPosition(LocalMeshPos);

                    // World Space -> Page Local Space
                    FVector3d PageLocalPos = PageTransform.InverseTransformPosition(WorldPos);

                    // Page Loccal -> UV Space
                    double NormX = (PageLocalPos.X - PageSize.Min.X) / PageSize.GetSize().X;
                    double NormY = (PageLocalPos.Y - PageSize.Min.Y) / PageSize.GetSize().Y;

                    Poly2D.AppendVertex(FVector2D(NormX, NormY));
                }

                // Track the largest loop found
                double AreaAbs = FMath::Abs(Poly2D.SignedArea());
                if (AreaAbs > LargestArea)
                {
                    LargestArea = AreaAbs;
                    OuterLoopIdx = i;
                }

                ParsedPolygons.Add(Poly2D);
            }

            // Assign largest loop as Outer, all remaining loops as Holes
            if (OuterLoopIdx != -1)
            {
                UE::Geometry::FPolygon2d OuterPoly = ParsedPolygons[OuterLoopIdx];

                // Outer boundary MUST be Counter-Clockwise bc UE geometry code expects this winding
                if (OuterPoly.IsClockwise())
                {
                    OuterPoly.Reverse();
                }
                EvidenceGenPoly.SetOuter(OuterPoly);

                for (int32 i = 0; i < ParsedPolygons.Num(); ++i)
                {
                    if (i != OuterLoopIdx)
                    {
                        UE::Geometry::FPolygon2d HolePoly = ParsedPolygons[i];

                        // Holes MUST be Clockwise
                        if (!HolePoly.IsClockwise())
                        {
                            HolePoly.Reverse();
                        }
                        EvidenceGenPoly.AddHole(HolePoly);
                    }
                }
            }
        });

    // 2. Validate extracted polygon
    if (EvidenceGenPoly.GetOuter().VertexCount() < 3) return;

    // Build AABB for fast broad-phase culling before running boolean ops
    // Might need to go further (trees) if we have slowdown
    UE::Geometry::FAxisAlignedBox2d PathBounds = EvidenceGenPoly.GetOuter().Bounds();
    FBox2D PathAABB(FVector2D(PathBounds.Min.X, PathBounds.Min.Y), FVector2D(PathBounds.Max.X, PathBounds.Max.Y));

    // 3. Process each Area
    for (const FScrapbookArea& Area : Page.Areas)
    {
        FBox2D AreaAABB(Area.Position, Area.Position + Area.Size);

        // AABB Culling
        if (PathAABB.Intersect(AreaAABB))
        {
            // Build Area Box as a General Polygon (Clockwise 4-point loop)
            UE::Geometry::FPolygon2d AreaPoly;
            AreaPoly.AppendVertex(Area.Position);
            AreaPoly.AppendVertex(FVector2D(Area.Position.X + Area.Size.X, Area.Position.Y));
            AreaPoly.AppendVertex(Area.Position + Area.Size);
            AreaPoly.AppendVertex(FVector2D(Area.Position.X, Area.Position.Y + Area.Size.Y));

            UE::Geometry::FGeneralPolygon2d AreaGenPoly(AreaPoly);

            // 4. Compute 2D Boolean Intersection using TBooleanPolygon2Polygon2
            TArray<UE::Geometry::FGeneralPolygon2d> IntersectionResults;
            
            using FPolygonIntersection = UE::Geometry::TBooleanPolygon2Polygon2<UE::Geometry::EPolygonBooleanOp::Intersect, UE::Geometry::FGeneralPolygon2d, double>;

            FPolygonIntersection IntersectionOp( EvidenceGenPoly, AreaGenPoly );

            if (IntersectionOp.ComputeResult())
            {
                IntersectionResults = IntersectionOp.Result;
            }

            // DEBUG: Draw the intersection result
            if (DrawDebug)
            {
                UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
                for (const UE::Geometry::FGeneralPolygon2d& ResultPoly : IntersectionResults)
                {
                    auto DrawPolygon = [&](const UE::Geometry::TPolygon2<double>& Poly)
                        {
                            TArray<FVector> ReconstructedWorldPoints;
                            for (FVector2D UV : Poly.GetVertices())
                            {
                                // Convert Normalized UV -> World Space
                                FVector ReconstructedWorldPos = Static_NormalizedToWorld(UV, PageSize, PageTransform);
                                ReconstructedWorldPoints.Add(ReconstructedWorldPos);
                            }

                            // Draw connecting lines for the loop
                            for (int32 i = 0; i < ReconstructedWorldPoints.Num(); ++i)
                            {
                                FVector PtA = ReconstructedWorldPoints[i];
                                FVector PtB = ReconstructedWorldPoints[(i + 1) % ReconstructedWorldPoints.Num()];
                                DrawDebugLine(World, PtA, PtB, FColor::Orange, false, 5.0f, 0, 2.5f);
                            }
                        };

                    DrawPolygon(ResultPoly.GetOuter());
                    for (auto& Hole : ResultPoly.GetHoles())
                        DrawPolygon(Hole);
                }
            }

            // 5. Calculate total clipped area across resulting polygons
            double TotalClippedArea = 0.0;
            for (const UE::Geometry::FGeneralPolygon2d& ResultPoly : IntersectionResults)
            {
                double AreaVal = ResultPoly.SignedArea();
                TotalClippedArea += AreaVal;
            }

            // 6. Compute ratio and output trait
            float OriginalAreaSize = Area.Size.X * Area.Size.Y;
            if (OriginalAreaSize > 0.0f && TotalClippedArea > 0.0)
            {
                float Ratio = FMath::Clamp(static_cast<float>(TotalClippedArea / OriginalAreaSize), 0.0f, 1.0f);

                if (Ratio > 0.001f) // Filter out tiny floating-point artifacts
                {
                    FEvidenceTrait NewTrait = Area.Trait;
                    NewTrait.Magnitude = FMath::RoundToInt(Area.Trait.Magnitude * Ratio);
                    ClippedTraits.Add(NewTrait);
                }
            }
        }
    }
}

// I didn't write this helper, but it does layout for the evidence pieces
struct FPackedActorInfo
{
    AActor* Actor;
    FBox SpatialBounds;
    float FootprintArea;
};

void UScrapbookingFunctionLibrary::LayoutActorsInTightSpiral(TArray<AActor*> TargetActors, float Padding, FVector CenterLocation)
{
    if (TargetActors.Num() == 0) return;

    TArray<FPackedActorInfo> SortedActors;
    SortedActors.Reserve(TargetActors.Num());

    // 1. Calculate world bounds and footprint area for each actor
    for (AActor* Actor : TargetActors)
    {
        if (!Actor) continue;

        FBox ActorBounds = Actor->GetComponentsBoundingBox(true);
        FVector Size = ActorBounds.GetSize();
        float Area = Size.X * Size.Y;

        FPackedActorInfo Info;
        Info.Actor = Actor;
        Info.SpatialBounds = ActorBounds;
        Info.FootprintArea = Area;

        SortedActors.Add(Info);
    }

    // 2. Sort the array from LARGEST footprint to SMALLEST
    SortedActors.Sort([](const FPackedActorInfo& A, const FPackedActorInfo& B) {
        return A.FootprintArea > B.FootprintArea;
        });

    TArray<FPackedActorInfo> PlacedActors;
    PlacedActors.Reserve(SortedActors.Num());

    // 3. Place the largest actor right at the target CenterLocation
    FPackedActorInfo& CenterActor = SortedActors[0];
    FVector CenterOffset = CenterActor.SpatialBounds.GetCenter() - CenterActor.Actor->GetActorLocation();

    // CHANGED: We now offset from CenterLocation instead of FVector::ZeroVector
    FVector FirstPosition = CenterLocation - CenterOffset;

    CenterActor.Actor->SetActorLocation(FirstPosition);
    CenterActor.SpatialBounds = CenterActor.Actor->GetComponentsBoundingBox(true);
    PlacedActors.Add(CenterActor);

    const float AngleStep = 0.05f;
    const float RadiusStep = 5.0f;

    // 4. Place every subsequent actor using a spiral search loop
    for (int32 i = 1; i < SortedActors.Num(); ++i)
    {
        FPackedActorInfo& Current = SortedActors[i];
        FVector CurrentSize = Current.SpatialBounds.GetSize();
        FVector LocalCenterOffset = Current.SpatialBounds.GetCenter() - Current.Actor->GetActorLocation();

        float CurrentRadius = 10.0f;
        float CurrentAngle = 0.0f;
        bool bPositionFound = false;
        FVector EvaluatedLocation = FVector::ZeroVector;

        while (!bPositionFound)
        {
            float TargetX = CurrentRadius * FMath::Cos(CurrentAngle);
            float TargetY = CurrentRadius * FMath::Sin(CurrentAngle);

            // CHANGED: Add CenterLocation to the local spiral offsets to shift the search window
            FVector TargetWorldCenter = CenterLocation + FVector(TargetX, TargetY, 0.0f);

            FBox TestBox(TargetWorldCenter - (CurrentSize * 0.5f), TargetWorldCenter + (CurrentSize * 0.5f));
            TestBox = TestBox.ExpandBy(FVector(Padding, Padding, 0.0f));

            bool bHasOverlap = false;

            for (const FPackedActorInfo& Placed : PlacedActors)
            {
                if (TestBox.Intersect(Placed.SpatialBounds))
                {
                    bHasOverlap = true;
                    break;
                }
            }

            if (!bHasOverlap)
            {
                EvaluatedLocation = TargetWorldCenter - LocalCenterOffset;
                bPositionFound = true;
            }
            else
            {
                CurrentAngle += AngleStep;
                CurrentRadius += (RadiusStep * (AngleStep / (2.0f * PI)));
            }
        }

        Current.Actor->SetActorLocation(EvaluatedLocation);
        Current.SpatialBounds = Current.Actor->GetComponentsBoundingBox(true);
        PlacedActors.Add(Current);
    }
}

// Trying to roll my own version of this as the geometry script BP function crashes in packaged builds for some reason
UDynamicMesh* UScrapbookingFunctionLibrary::AppendSimpleExtrudeLoop(UDynamicMesh* TargetMesh, const TArray<FVector2D>& PolygonVertices, float Height)
{
    using namespace UE::Geometry;

    if (TargetMesh == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("AppendSimpleExtrudeLoop: TargetMesh is Null"));
        return TargetMesh;
    }
    if (PolygonVertices.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("AppendSimpleExtrudeLoop: PolygonVertices array requires at least 3 positions"));
        return TargetMesh;
    }

    // Build the cross-section loop from the input 2D vertices
    FGeneralizedCylinderGenerator ExtrudeGen;
    for (const FVector2D& Point : PolygonVertices)
    {
        ExtrudeGen.CrossSection.AppendVertex(FVector2d(Point));
    }

    // Straight path extrusion
    ExtrudeGen.Path.Add(FVector3d(0, 0, Height * -0.5));
    ExtrudeGen.Path.Add(FVector3d(0, 0, Height * 0.5));

    ExtrudeGen.InitialFrame = FFrame3d();
    ExtrudeGen.bCapped = true;

    ExtrudeGen.Generate();

    // Convert generator output to a FDynamicMesh3 and append it into TargetMesh
    FDynamicMesh3 GeneratedMesh(&ExtrudeGen);

    TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh)
        {
            FMeshIndexMappings Mappings;
            FDynamicMeshEditor Editor(&EditMesh);
            Editor.AppendMesh(&GeneratedMesh, Mappings);
        });

    return TargetMesh;
}

bool UScrapbookingFunctionLibrary::DoPolygonsIntersect(const TArray<FVector2D>& A, const TArray<FVector2D>& B)
{
    using namespace UE::Geometry;
    
    TPolygon2<double> AP(A);
    TPolygon2<double> BP(B);
    return AP.Intersects(BP);
}

TArray<AActor*> UScrapbookingFunctionLibrary::ActorLocationSort(const TArray<AActor*>& Array)
{
    TArray<AActor*> Ret(Array);
    Ret.Sort([](const AActor& A, const AActor& B) { 
        FVector PosA = A.GetActorLocation();
        FVector PosB = B.GetActorLocation();

        float Tol = 20.0f;

        if (!FMath::IsNearlyEqual(PosA.X, PosB.X, Tol))
        {
            return PosA.X > PosB.X; 
        }

        return PosA.Y < PosB.Y;
    });
    return Ret;
}

static void ClearExistingSetsForEvidence( AActor* ActorA, AActor* ActorB, TArray<FEvidenceSet>& EvidenceSets )
{
    TArray<FEvidenceSet> FilteredSets;
    for (auto& Set : EvidenceSets)
    {
        if ( !( Set.Evidence.Contains(ActorA) || Set.Evidence.Contains(ActorB) ) )
        {
            FilteredSets.Add(Set);
        }
    }
    EvidenceSets = FilteredSets;
}

static FEvidenceSet& GetOrCreateEvidenceSet( AActor* Actor, const TArray<FEvidenceTrait>& Traits, TArray<FEvidenceSet>& EvidenceSets )
{
    for (auto& Set : EvidenceSets)
    {
        if ( Set.Evidence.Contains( Actor ) )
        {
            return Set;
        }
    }

    FEvidenceSet NewSet;
    NewSet.Evidence.Add(Actor);
    NewSet.Traits = Traits;
    return EvidenceSets[ EvidenceSets.Add(NewSet) ];
}

UE_DISABLE_OPTIMIZATION

TArray<FEvidenceTrait> UScrapbookingFunctionLibrary::GetTraitProductsFromInteractions(const TArray<FEvidenceTrait>& A, const TArray<FEvidenceTrait>& B, AActor* ActorA, AActor* ActorB, const TArray<FEvidenceTraitInteraction>& Rules, const FVector& Location, TArray<FEvidenceSet>& EvidenceSets, TArray<FEvidenceTraitInteractionResult>& RuleApplicationResults)
{
    TArray<FEvidenceTrait> Products;

    // Overlapping actors should come from two different sets

    const FEvidenceSet& SetA = GetOrCreateEvidenceSet( ActorA, A, EvidenceSets );
    const FEvidenceSet& SetB = GetOrCreateEvidenceSet( ActorB, B, EvidenceSets );

    Products.Append(SetA.Traits);
    Products.Append(SetB.Traits);

    for (const FEvidenceTraitInteraction& Rule : Rules)
    {
        switch (Rule.InteractionType)
        {
            case EEvidenceTraitInteractionType::Conversion:
            {
                auto APtr = SetA.Traits.FindByPredicate([Rule](const FEvidenceTrait& T) { return T.TypeName == Rule.InputTypeNameA; });
                auto BPtr = SetB.Traits.FindByPredicate([Rule](const FEvidenceTrait& T) { return T.TypeName == Rule.InputTypeNameB; });
                if (APtr && BPtr)
                {
                    int ResultAmount = FMath::RoundToInt((APtr->Magnitude / 100.f) * BPtr->Magnitude );
                    Products.Add( FEvidenceTrait(Rule.InputTypeNameB, -ResultAmount ) );
                    Products.Add( FEvidenceTrait(Rule.OutputTypeNameC, ResultAmount ) );
                    RuleApplicationResults.Add( FEvidenceTraitInteractionResult( Rule, { APtr->Magnitude, BPtr->Magnitude, ResultAmount }, Location) );
                }
                continue;
            }

            case EEvidenceTraitInteractionType::Combination:
            {
                auto APtr = SetA.Traits.FindByPredicate([Rule](const FEvidenceTrait& T) { return T.TypeName == Rule.InputTypeNameA; });
                auto BPtr = SetB.Traits.FindByPredicate([Rule](const FEvidenceTrait& T) { return T.TypeName == Rule.InputTypeNameB; });

                // Try the other order
                if (!(APtr && BPtr))
                {
                    APtr = SetB.Traits.FindByPredicate([Rule](const FEvidenceTrait& T) { return T.TypeName == Rule.InputTypeNameA; });
                    BPtr = SetA.Traits.FindByPredicate([Rule](const FEvidenceTrait& T) { return T.TypeName == Rule.InputTypeNameB; });
                }

                if (APtr && BPtr)
                {
                    int ProductMagnitude = FMath::Min(APtr->Magnitude, BPtr->Magnitude);
                    Products.Add(FEvidenceTrait(Rule.InputTypeNameA, -ProductMagnitude));
                    Products.Add(FEvidenceTrait(Rule.InputTypeNameB, -ProductMagnitude));
                    Products.Add(FEvidenceTrait(Rule.OutputTypeNameC, ProductMagnitude));
                    RuleApplicationResults.Add(FEvidenceTraitInteractionResult(Rule, { APtr->Magnitude, BPtr->Magnitude, ProductMagnitude }, Location));
                }
                continue;
            }
            
            // More rule types...
        }
    }

    // Merge evidence sets and use the new product
    Products = SumTraits(Products);

    FEvidenceSet NewSet;
    NewSet.Evidence.Append(SetA.Evidence);
    NewSet.Evidence.Append(SetB.Evidence);
    NewSet.Traits.Append(Products);

    ClearExistingSetsForEvidence(ActorA, ActorB, EvidenceSets);
    EvidenceSets.Add(NewSet);

    return Products;
}

UE_ENABLE_OPTIMIZATION

TArray<FEvidenceTrait> UScrapbookingFunctionLibrary::SumTraits(const TArray<FEvidenceTrait>& Input)
{
    TMap<FName, int> Tally;

    for (auto& Trait : Input)
    {
        Tally.FindOrAdd(Trait.TypeName) += Trait.Magnitude;
    }

    TArray<FEvidenceTrait> Result;
    for (auto& Entry : Tally)
    {
        if ( Entry.Value > 0 )
        {
            Result.Add(FEvidenceTrait(Entry.Key, Entry.Value));
        }
    }

    return Result;
}

// ============================
//*****************************
// Editor stuff
//*****************************
// ============================
void UScrapbookingFunctionLibrary::CopyToClipboard(const FString& TextToCopy)
{
    FPlatformApplicationMisc::ClipboardCopy(*TextToCopy);
}

FString UScrapbookingFunctionLibrary::ReadFromClipboard()
{
    FString ClipboardText;
    FPlatformApplicationMisc::ClipboardPaste(ClipboardText);
    return ClipboardText;
}

FString UScrapbookingFunctionLibrary::ConvertPageStructToUnrealText(const FScrapbookPage& PageData)
{
    FString OutputText;
    FScrapbookPage DefaultPage;
    FScrapbookPage::StaticStruct()->ExportText(
        OutputText,
        &PageData,
        &DefaultPage,
        nullptr,
        PPF_Copy,
        nullptr
    );

    return OutputText;
}

FString UScrapbookingFunctionLibrary::ConvertInteractionsToUnrealText(const TArray<FEvidenceTraitInteraction>& Rules)
{
    TArray<FString> AllRules;
    for (const auto& Rule : Rules)
    {
        FString OutputText;
        FEvidenceTraitInteraction Default;
        FEvidenceTraitInteraction::StaticStruct()->ExportText(
            OutputText,
            &Rule,
            &Default,
            nullptr,
            PPF_Copy,
            nullptr
        );
        AllRules.Add(OutputText);
    }

    return FString::Format(TEXT("( {0} )"), { FString::Join(AllRules, TEXT(",")) });
}

bool UScrapbookingFunctionLibrary::ConvertUnrealTextToPageStruct(const FString& UnrealText, FScrapbookPage& OutPage)
{
    // Start with a clean slate
    OutPage = FScrapbookPage();

    if (UnrealText.IsEmpty())
    {
        return false;
    }

    // A device to catch any syntax or parsing errors generated by Unreal's reflection system
    FStringOutputDevice ErrorLog;

    // Ask the struct's reflection data to import the text string
    const TCHAR* ParsedBuffer = FScrapbookPage::StaticStruct()->ImportText(
        *UnrealText,                 // The buffer to read from
        &OutPage,                    // The struct memory to populate
        nullptr,                     // Parent Object (none)
        PPF_Copy,                    // PortFlags: Must match what was used during ExportText
        &ErrorLog,                   // Error output device
        FScrapbookPage::StaticStruct()->GetName() // Scope / Error context name
    );

    // ImportText returns a pointer to the remaining text if successful, or a nullptr if it failed
    if (ParsedBuffer == nullptr)
    {
        return false;
    }

    return true;
}

bool UScrapbookingFunctionLibrary::ConvertUnrealTextToInteractionList(const FString& UnrealText, TArray<FEvidenceTraitInteraction>& Rules)
{
    // Start with a clean slate
    Rules.Empty();

    if (UnrealText.IsEmpty())
    {
        return false;
    }

    FStringOutputDevice ErrorLog;

    // 1. Create a temporary Array Property
    // FFieldVariant() is an empty owner, since this property doesn't live on a UCLASS or USTRUCT.
    FArrayProperty* TempArrayProp = new FArrayProperty(FFieldVariant(), TEXT("TempArrayProp"));

    // 2. Create the inner Struct Property that tells the array what type of data it holds
    FStructProperty* InnerStructProp = new FStructProperty(TempArrayProp, TEXT("TempInnerProp"));
    InnerStructProp->Struct = FEvidenceTraitInteraction::StaticStruct();
    InnerStructProp->SetElementSize(InnerStructProp->Struct->GetStructureSize());

    // 3. Link the inner struct to the array
    TempArrayProp->Inner = InnerStructProp;

    // 4. Ask the Array Property to import the text directly into the Rules array memory
    const TCHAR* ParsedBuffer = TempArrayProp->ImportText_Direct(
        *UnrealText,                 // The buffer to read from
        &Rules,                      // The memory address of the TArray
        nullptr,                     // Parent Object (none)
        PPF_Copy,                    // PortFlags: Must match what was used during ExportText
        &ErrorLog                    // Error output device
    );

    // 5. Clean up temporary reflection data
    // Note: Deleting an FArrayProperty automatically calls 'delete Inner' internally, 
    // so we only need to delete the parent property to avoid a memory leak.
    delete TempArrayProp;

    // ImportText_Direct returns a pointer to the remaining text if successful, or a nullptr if it failed
    if (ParsedBuffer == nullptr)
    {
        // Optionally, log out the contents of ErrorLog here
        return false;
    }

    return true;
}

static bool OpenFileDialog(
    const FString& DialogTitle,
    const FString& DefaultPath,
    const FString& FileTypes,
    FString& OutSelectedFilePath
)
{
#if PLATFORM_WINDOWS
    OPENFILENAMEW Ofn;
    wchar_t SzFile[MAX_PATH] = { 0 };

    // Convert string inputs to TCHAR/WCHAR format for Windows API
    FString CleanDefaultPath = FPaths::ConvertRelativePathToFull(DefaultPath);
    CleanDefaultPath.ReplaceInline(TEXT("/"), TEXT("\\"));

    // Build the Windows API filter string (Format: "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0\0")
    // Input format expected: "Text Files (*.txt)|*.txt|All Files (*.*)|*.*"
    FString WindowsFilter = FileTypes;
    WindowsFilter.ReplaceInline(TEXT("|"), TEXT("\0"), ESearchCase::CaseSensitive);
    WindowsFilter.AppendChar(TEXT('\0')); // Double null-terminate

    ZeroMemory(&Ofn, sizeof(Ofn));
    Ofn.lStructSize = sizeof(Ofn);

    // Get active window handle or pass nullptr
    Ofn.hwndOwner = (HWND)FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle();
    Ofn.lpstrFile = SzFile;
    Ofn.nMaxFile = sizeof(SzFile) / sizeof(wchar_t);
    Ofn.lpstrFilter = *WindowsFilter;
    Ofn.nFilterIndex = 1;
    Ofn.lpstrTitle = *DialogTitle;
    Ofn.lpstrInitialDir = CleanDefaultPath.IsEmpty() ? nullptr : *CleanDefaultPath;
    Ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&Ofn))
    {
        OutSelectedFilePath = FString(SzFile);
        return true;
    }
#endif

    return false;
}

UTexture2D* UScrapbookingFunctionLibrary::LoadPNGViaFileDialog( FString DialogTitle )
{
    FString SelectedFilePath;
    bool bOpened = false;

#if PLATFORM_WINDOWS
    OPENFILENAMEW Ofn;
    wchar_t SzFile[MAX_PATH] = { 0 };

    // Win32 API filter format: Description\0Pattern\0\0
    const wchar_t Filter[] = L"PNG Image (*.png)\0*.png\0";

    // Grab the active window handle so the dialog blocks input properly
    HWND HwndParent = nullptr;
    if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetActiveTopLevelWindow().IsValid())
    {
        TSharedPtr<FGenericWindow> NativeWindow = FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow();
        if (NativeWindow.IsValid())
        {
            HwndParent = static_cast<HWND>(NativeWindow->GetOSWindowHandle());
        }
    }

    ZeroMemory(&Ofn, sizeof(Ofn));
    Ofn.lStructSize = sizeof(Ofn);
    Ofn.hwndOwner = HwndParent;
    Ofn.lpstrFile = SzFile;
    Ofn.nMaxFile = sizeof(SzFile) / sizeof(wchar_t);
    Ofn.lpstrFilter = Filter;
    Ofn.nFilterIndex = 1;
    Ofn.lpstrTitle = *DialogTitle;
    Ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&Ofn))
    {
        SelectedFilePath = FString(SzFile);
        bOpened = true;
    }
#endif

    if (!bOpened || SelectedFilePath.IsEmpty())
    {
        return nullptr;
    }

    UTexture2D* NewTexture = FImageUtils::ImportFileAsTexture2D(SelectedFilePath);
    return NewTexture;
}
